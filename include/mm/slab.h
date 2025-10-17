#ifndef MM_SLAB_H
#define MM_SLAB_H

#include <kernel/kernel.h>
#include <misc/list.h>

#define SLAB_MIN_ALIGN 8
#define SLAB_FLAGS_NONE 0

typedef void (*ctor_t)(void *);

struct kmem_cache {
  const char *name;
  u32 object_size;
  u32 align;
  u32 flags;
  ctor_t ctor;
  u32 objects_per_slab;
  struct list_head slabs_full;
  struct list_head slabs_partial;
  struct list_head slabs_free;
  struct list_head list; /* For global cache chain */
};

struct slab {
  struct kmem_cache *cache;
  void *freelist; /* Head of free object linked list */
  u32 inuse;      /* Number of allocated objects */
  struct list_head list;
};

kernel_status_t slab_init(void);
struct kmem_cache *kmem_cache_create(const char *name, u32 size, u32 align,
                                     u32 flags, ctor_t ctor);
void kmem_cache_destroy(struct kmem_cache *cache);
void *kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);

extern struct list_head kmem_caches;
#endif