#include <assert.h>
#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <string.h>

/* we need these prototypes for page/phys manipulation */
extern u32 pmm_alloc_page(void);
extern void pmm_free_page(u32 addr);
extern u32 vmm_get_physical_addr(u32 virt_addr);
extern kernel_status_t vmm_unmap_page(u32 virt_addr);

struct list_head kmem_caches;

/* Helpers */
static inline uintptr_t page_base_from_ptr(const void *p)
{
	return (uintptr_t)p & ~(PAGE_SIZE - 1);
}

static inline struct slab *obj_to_slab(void *obj)
{
	return (struct slab *)page_base_from_ptr(obj);
}

static inline void *get_next_free(void *obj)
{
	return *(void **)obj;
}

static inline void set_next_free(void *obj, void *next)
{
	*(void **)obj = next;
}

/* Allocate and initialize a new slab (one page) for the given cache.
 * On success returns 0 and slab is added to cache->slabs_partial.
 */
static int new_slab(struct kmem_cache *cache)
{
	uint32_t phys = pmm_alloc_page();
	if (!phys)
		return -1;

	/* Map the page into the kernel virtual space (identity mapping) */
	uintptr_t virt = (uintptr_t)phys;
	int ret = vmm_map_page(virt, phys,
			       PAGE_FLAG_PRESENT | PAGE_FLAG_RW
				       | PAGE_FLAG_GLOBAL);
	if (ret != 0) {
		pmm_free_page(phys);
		return -1;
	}

	struct slab *slab = (struct slab *)virt;
	memset(slab, 0, sizeof(*slab));

	slab->cache = cache;
	INIT_LIST_HEAD(&slab->list);
	slab->inuse = 0;

	/* Build free list: objects are placed after the slab struct,
   * aligned to cache->align.
   */
	uintptr_t obj_off
		= ALIGN_UP((uintptr_t)virt + sizeof(*slab), cache->align);
	void *prev = NULL;

	for (uint32_t i = 0; i < cache->objects_per_slab; i++) {
		void *obj
			= (void *)(obj_off + (uintptr_t)i * cache->object_size);
		/* link objects in LIFO order */
		set_next_free(obj, prev);
		prev = obj;
	}

	slab->freelist = prev;

	/* add to partial list */
	list_add_tail(&slab->list, &cache->slabs_partial);

	return 0;
}

/* Initialize the slab subsystem */
int slab_init(void)
{
	INIT_LIST_HEAD(&kmem_caches);
	log(LOG_OKAY, "SLAB: initialized");
	return 0;
}

/* Create a kmem cache. Returns NULL on failure. */
struct kmem_cache *kmem_cache_create(const char *name, uint32_t size,
				     uint32_t align, uint32_t flags,
				     ctor_t ctor)
{
	/* Validation */
	if (!name || size == 0 || align < SLAB_MIN_ALIGN
	    || (align & (align - 1)) != 0) {
		log(LOG_WARN, "SLAB: invalid arguments to kmem_cache_create");
		return NULL;
	}

	/* Allocate a page for the cache metadata */
	uint32_t phys = pmm_alloc_page();
	if (!phys) {
		log(LOG_WARN, "SLAB: out of memory while creating cache '%s'",
		    name);
		return NULL;
	}

	uintptr_t virt = (uintptr_t)phys;
	if (vmm_map_page(virt, phys,
			 PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_GLOBAL)
	    != 0) {
		pmm_free_page(phys);
		log(LOG_WARN, "SLAB: vmm_map_page failed for cache '%s'", name);
		return NULL;
	}

	/* Place the kmem_cache at the start of the page */
	struct kmem_cache *cache = (struct kmem_cache *)virt;
	memset(cache, 0, sizeof(*cache));

	/* Copy the name into space after the cache struct if it fits */
	size_t name_len = strlen(name) + 1;
	if (sizeof(*cache) + name_len > PAGE_SIZE) {
		vmm_unmap_page(virt);
		pmm_free_page(phys);
		log(LOG_WARN, "SLAB: cache name too long for '%s'", name);
		return NULL;
	}

	char *name_copy = (char *)(cache + 1);
	memcpy(name_copy, name, name_len);
	cache->name = name_copy;

	/* Compute aligned object size and count of objects per slab */
	cache->align = align;
	cache->object_size = ALIGN_UP(size, align);
	cache->flags = flags;
	cache->ctor = ctor;

	/* Determine how many objects fit in a single page slab */
	uintptr_t first_obj_off
		= ALIGN_UP((uintptr_t)virt + sizeof(struct slab), align);
	uint32_t avail = PAGE_SIZE - (first_obj_off - virt);
	cache->objects_per_slab = avail / cache->object_size;

	if (cache->objects_per_slab == 0) {
		vmm_unmap_page(virt);
		pmm_free_page(phys);
		log(LOG_WARN, "SLAB: object too large for page (cache=%s)",
		    name);
		return NULL;
	}

	INIT_LIST_HEAD(&cache->slabs_full);
	INIT_LIST_HEAD(&cache->slabs_partial);
	INIT_LIST_HEAD(&cache->slabs_free);
	INIT_LIST_HEAD(&cache->list);

	/* Add cache to global list */
	list_add_tail(&cache->list, &kmem_caches);

	log(LOG_OKAY, "SLAB: created cache '%s' obj_size=%u objs_per_slab=%u",
	    cache->name, cache->object_size, cache->objects_per_slab);

	return cache;
}

/* Destroy a cache and free all slabs (best-effort). */
void kmem_cache_destroy(struct kmem_cache *cache)
{
	if (!cache)
		return;

	/* Warn if there are in-use objects in full/partial lists */
	bool inuse = !list_empty(&cache->slabs_full)
		     || !list_empty(&cache->slabs_partial);
	if (inuse) {
		log(LOG_WARN, "SLAB: destroying cache '%s' with in-use objects",
		    cache->name);
	}

	/* Remove from global cache list */
	list_del(&cache->list);

	/* Free all slabs in all lists */
	struct list_head *lists[] = {&cache->slabs_full, &cache->slabs_partial,
				     &cache->slabs_free};
	for (size_t li = 0; li < sizeof(lists) / sizeof(lists[0]); li++) {
		struct list_head *head = lists[li];

		while (!list_empty(head)) {
			struct slab *slab
				= list_first_entry(head, struct slab, list);
			list_del(&slab->list);

			uintptr_t virt = (uintptr_t)slab;
			uint32_t phys = vmm_get_physical_addr((u32)virt) & ~0xFFF;
			vmm_unmap_page((u32)virt);
			pmm_free_page(phys);
		}
	}

	/* Free the cache metadata page itself */
	{
		uintptr_t virt_cache = (uintptr_t)cache;
		uint32_t phys_cache = vmm_get_physical_addr((u32)virt_cache);
		vmm_unmap_page((u32)virt_cache);
		pmm_free_page(phys_cache);
	}
}

/* Try to reclaim/free slabs on the slabs_free list back to PMM.
 * Best-effort: frees fully-empty slabs. Leaves partial/full ones alone.
 */
void kmem_cache_shrink(struct kmem_cache *cache)
{
	if (!cache)
		return;

	struct list_head *head = &cache->slabs_free;

	/* Iterate while there are free slabs; remove and free them */
	while (!list_empty(head)) {
		struct slab *slab = list_first_entry(head, struct slab, list);
		list_del(&slab->list);

		uintptr_t virt = (uintptr_t)slab;
		/* get physical page for this slab and unmap then free it */
		uint32_t phys = vmm_get_physical_addr((u32)virt) & ~0xFFF;
		vmm_unmap_page((u32)virt);
		pmm_free_page(phys);
		/* continue to next slab */
	}
}

/* Allocate one object from the cache. Returns NULL on failure. */
void *kmem_cache_alloc(struct kmem_cache *cache)
{
	if (!cache)
		return NULL;

	/* Prefer partial slabs, fall back to free list */
	struct list_head *slab_list = !list_empty(&cache->slabs_partial)
					      ? &cache->slabs_partial
					      : &cache->slabs_free;

	if (list_empty(slab_list)) {
		/* Need a new slab */
		if (new_slab(cache) != 0)
			return NULL;
		slab_list = &cache->slabs_partial;
	}

	struct slab *slab = list_first_entry(slab_list, struct slab, list);
	/* Pop an object from the freelist */
	void *obj = slab->freelist;
	assert(obj != NULL);
	slab->freelist = get_next_free(obj);
	slab->inuse++;

	/* Zero the object area before returning it */
	memset(obj, 0, cache->object_size);

	/* Call constructor if present (constructor may use the object) */
	if (cache->ctor)
		cache->ctor(obj);

	/* If slab became full, move it to full list */
	if (slab->inuse == cache->objects_per_slab) {
		list_move_tail(&slab->list, &cache->slabs_full);
	}

	return obj;
}

/* Free an object back to its cache. */
void kmem_cache_free(struct kmem_cache *cache, void *obj)
{
	if (!cache || !obj)
		return;

	struct slab *slab = obj_to_slab(obj);

	/* Basic sanity check: ensure the object belongs to the cache. */
	if (slab->cache != cache) {
		log(LOG_WARN,
		    "SLAB: object %p does not belong to cache '%s' (possible "
		    "leak or "
		    "corruption)",
		    obj, cache ? cache->name : "(null)");
		return;
	}

	/* Push object back onto freelist */
	set_next_free(obj, slab->freelist);
	slab->freelist = obj;
	if (slab->inuse == 0) {
		/* shouldn't happen, but guard against underflow */
		log(LOG_WARN, "SLAB: double free detected for cache '%s'",
		    cache->name);
		return;
	}
	slab->inuse--;

	/* Move slab between lists depending on occupancy */
	list_del_init(&slab->list);
	if (slab->inuse == 0)
		list_add_tail(&slab->list, &cache->slabs_free);
	else
		list_add_tail(&slab->list, &cache->slabs_partial);
}