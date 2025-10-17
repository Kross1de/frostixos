#include <misc/logger.h>
#include <mm/bitmap.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <string.h>

struct list_head kmem_caches;

static struct slab *obj_to_slab(void *obj) {
  return (struct slab *)((uintptr_t)obj & ~(PAGE_SIZE - 1));
}

static void *get_next_free(void *obj) { return *(void **)obj; }

static void set_next_free(void *obj, void *next) { *(void **)obj = next; }

static kernel_status_t new_slab(struct kmem_cache *cache) {
  u32 page_phys = pmm_alloc_page();
  if (page_phys == 0)
    return KERNEL_OUT_OF_MEMORY;

  u32 virt_addr = page_phys;
  kernel_status_t status =
      vmm_map_page(virt_addr, page_phys,
                   PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_GLOBAL);
  if (status != KERNEL_OK) {
    pmm_free_page(page_phys);
    return status;
  }

  struct slab *slab = (struct slab *)virt_addr;
  slab->cache = cache;
  slab->inuse = 0;
  INIT_LIST_HEAD(&slab->list);

  /* Initialize free list */
  uintptr_t obj_start = virt_addr + sizeof(struct slab);
  obj_start = ALIGN_UP(obj_start, cache->align);
  void *prev = NULL;

  for (u32 i = 0; i < cache->objects_per_slab; i++) {
    void *obj = (void *)(obj_start + i * cache->object_size);
    set_next_free(obj, prev);
    prev = obj;
  }
  slab->freelist = prev;

  /* Add to partial list initially */
  list_add(&slab->list, &cache->slabs_partial);
  return KERNEL_OK;
}

kernel_status_t slab_init(void) {
  INIT_LIST_HEAD(&kmem_caches);
  log(LOG_OKAY, "SLAB: Initialized");
  return KERNEL_OK;
}

struct kmem_cache *kmem_cache_create(const char *name, u32 size, u32 align,
                                     u32 flags, ctor_t ctor) {
  if (size == 0 || align < SLAB_MIN_ALIGN || (align & (align - 1)) != 0)
    return NULL;

  u32 phys = pmm_alloc_page();
  if (phys == 0)
    return NULL;

  kernel_status_t status = vmm_map_page(
      phys, phys, PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_GLOBAL);
  if (status != KERNEL_OK) {
    pmm_free_page(phys);
    return NULL;
  }

  struct kmem_cache *cache = (struct kmem_cache *)phys;
  memset(cache, 0, sizeof(struct kmem_cache));

  /* Copy the name into the page */
  size_t name_len = strlen(name) + 1; /* Include null terminator */
  if (sizeof(struct kmem_cache) + name_len > PAGE_SIZE) {
    vmm_unmap_page(phys);
    pmm_free_page(phys);
    return NULL;
  }
  char *name_copy = (char *)(cache + 1); /* Space after struct */
  memcpy(name_copy, name, name_len);
  cache->name = name_copy;

  cache->object_size = ALIGN_UP(size, align);
  cache->align = align;
  cache->flags = flags;
  cache->ctor = ctor;

  /* Calculate objects per slab, accounting for aligned offset */
  u32 offset = ALIGN_UP(sizeof(struct slab), align);
  u32 avail = PAGE_SIZE - offset;
  cache->objects_per_slab = avail / cache->object_size;

  if (cache->objects_per_slab == 0) {
    vmm_unmap_page(phys);
    pmm_free_page(phys);
    return NULL;
  }

  INIT_LIST_HEAD(&cache->slabs_full);
  INIT_LIST_HEAD(&cache->slabs_partial);
  INIT_LIST_HEAD(&cache->slabs_free);
  INIT_LIST_HEAD(&cache->list);
  list_add(&cache->list, &kmem_caches);

  log(LOG_OKAY, "SLAB: Created cache '%s' (obj_size=%u, objs_per_slab=%u)",
      name, cache->object_size, cache->objects_per_slab);
  return cache;
}

void kmem_cache_destroy(struct kmem_cache *cache) {
  if (cache == NULL)
    return;

  /* Check for in-use objects */
  struct list_head *lists[] = {&cache->slabs_full, &cache->slabs_partial};
  for (int i = 0; i < 2; i++) {
    if (!list_empty(lists[i])) {
      log(LOG_WARN, "SLAB: Destroying cache '%s' with in-use objects",
          cache->name);
      break;
    }
  }

  list_del(&cache->list);

  /* Free all slabs */
  struct list_head *all_lists[] = {&cache->slabs_full, &cache->slabs_partial,
                                   &cache->slabs_free};
  for (int i = 0; i < 3; i++) {
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, all_lists[i]) {
      struct slab *slab = list_entry(pos, struct slab, list);
      list_del(pos);
      u32 virt = (u32)slab;
      u32 phys = vmm_get_physical_addr(virt);
      vmm_unmap_page(virt);
      pmm_free_page(phys);
    }
  }

  /* Free cache struct */
  u32 virt_cache = (u32)cache;
  u32 phys_cache = vmm_get_physical_addr(virt_cache);
  vmm_unmap_page(virt_cache);
  pmm_free_page(phys_cache);
}

void *kmem_cache_alloc(struct kmem_cache *cache) {
  if (cache == NULL)
    return NULL;

  /* Find a slab with free objects: prefer partial */
  struct list_head *slab_list = list_empty(&cache->slabs_partial)
                                    ? &cache->slabs_free
                                    : &cache->slabs_partial;

  if (list_empty(slab_list)) {
    /* No free/partial: allocate new slab */
    kernel_status_t status = new_slab(cache);
    if (status != KERNEL_OK) {
      return NULL;
    }
    slab_list = &cache->slabs_partial;
  }

  struct slab *slab = list_first_entry(slab_list, struct slab, list);
  void *obj = slab->freelist;
  slab->freelist = get_next_free(obj);
  slab->inuse++;
  memset(obj, 0, cache->object_size);

  if (cache->ctor) {
    cache->ctor(obj);
  }

  /* Move slab if full */
  if (slab->inuse == cache->objects_per_slab) {
    list_del(&slab->list);
    list_add(&slab->list, &cache->slabs_full);
  }

  return obj;
}

void kmem_cache_free(struct kmem_cache *cache, void *obj) {
  if (cache == NULL || obj == NULL)
    return;

  struct slab *slab = obj_to_slab(obj);
  if (slab->cache != cache)
    return; /* Invalid */

  set_next_free(obj, slab->freelist);
  slab->freelist = obj;
  slab->inuse--;

  list_del(&slab->list);
  if (slab->inuse == 0) {
    list_add(&slab->list, &cache->slabs_free);
  } else {
    list_add(&slab->list, &cache->slabs_partial);
  }
}