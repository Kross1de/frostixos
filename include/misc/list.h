#ifndef MISC_LIST_H
#define MISC_LIST_H

#include <kernel/kernel.h>

struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

/* Initialize a list head */
#define INIT_LIST_HEAD(ptr)                                                    \
  do {                                                                         \
    (ptr)->next = (ptr);                                                       \
    (ptr)->prev = (ptr);                                                       \
  } while (0)

/* Add a node between two known nodes */
static inline void __list_add(struct list_head *new, struct list_head *prev,
                              struct list_head *next) {
  next->prev = new;
  new->next = next;
  new->prev = prev;
  prev->next = new;
}

/* Add a new entry to the head of the list */
static inline void list_add(struct list_head *new, struct list_head *head) {
  __list_add(new, head, head->next);
}

/* Add a new entry to the tail of the list */
static inline void list_add_tail(struct list_head *new,
                                 struct list_head *head) {
  __list_add(new, head->prev, head);
}

/* Delete a node between two known nodes */
static inline void __list_del(struct list_head *prev, struct list_head *next) {
  next->prev = prev;
  prev->next = next;
}

/* Delete an entry from the list */
static inline void list_del(struct list_head *entry) {
  __list_del(entry->prev, entry->next);
  entry->next = (struct list_head *)0;
  entry->prev = (struct list_head *)0;
}

/* Check if the list is empty */
static inline int list_empty(const struct list_head *head) {
  return head->next == head;
}

/* Get the struct for this entry */
#define list_entry(ptr, type, member)                                          \
  ((type *)((uintptr_t)(ptr) - (uintptr_t)(&((type *)0)->member)))

/* Iterate over the list */
#define list_for_each(pos, head)                                               \
  for (pos = (head)->next; pos != (head); pos = pos->next)

/* Iterate over the list safely */
#define list_for_each_safe(pos, n, head)                                       \
  for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

/* Get the first entry in the list */
#define list_first_entry(ptr, type, member)                                    \
  list_entry((ptr)->next, type, member)

#endif