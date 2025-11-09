#ifndef MM_SLAB_H
#define MM_SLAB_H

#include <kernel/kernel.h>
#include <misc/list.h>
#include <mm/heap.h>

/*
 * Simple slab allocator API.
 *
 * A kmem_cache describes a cache of objects of a fixed size. Slabs
 * represent individual pages partitioned into objects.
 */

#define SLAB_MIN_ALIGN 8u
#define SLAB_FLAGS_NONE 0u

typedef void (*ctor_t)(void *);

/* Forward declarations */
struct kmem_cache;
struct slab;

/* kmem_cache structure describing a cache of objects */
struct kmem_cache {
	/* immutable after creation */
	const char *name;
	u32 object_size;	/* size actually used for allocations */
	u32 align;		/* object alignment (power of two) */
	u32 flags;
	ctor_t ctor;

	/* derived: objects per slab */
	u32 objects_per_slab;

	/* slab lists (full/partial/free) */
	struct list_head slabs_full;
	struct list_head slabs_partial;
	struct list_head slabs_free;

	/* node for global caches list */
	struct list_head list;
};

/*
 * slab structure represents metadata stored at page beginning for
 * a slab. 'freelist' points to the first free object within the slab.
 */
struct slab {
	struct kmem_cache *cache; /* owning cache */
	void *freelist;	     /* head of free object linked list */
	u32 inuse;		     /* number of allocated objects */
	struct list_head list;	     /* node in cache slab lists */
};

/* Global list of caches */
extern struct list_head kmem_caches;

/* Core slab API */
int slab_init(void);
struct kmem_cache *kmem_cache_create(const char *name, uint32_t size,
				     uint32_t align, uint32_t flags,
				     ctor_t ctor);
void kmem_cache_destroy(struct kmem_cache *cache);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

/* Try to shrink the cache by freeing slabs on its slabs_free list back to
 * the physical allocator. Best-effort and may leave some slabs for reuse.
 */
void kmem_cache_shrink(struct kmem_cache *cache);

#endif /* MM_SLAB_H */