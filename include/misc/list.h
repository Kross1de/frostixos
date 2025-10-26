#ifndef MISC_LIST_H
#define MISC_LIST_H

#include <kernel/kernel.h>
#include <stddef.h>

/*
 * Doubly linked circular list implementation.
 *
 * struct list_head:
 *	- next: pointer to next element in list
 *	- prev: pointer to previous element in list
 *
 * The list head is a plain node that points to itself when empty.
 */

/* List node */
struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

/* Initialize a list head to an empty list. */
static inline void INIT_LIST_HEAD(struct list_head *head)
{
	head->next = head;
	head->prev = head;
}

/*
 * Internal helper: link 'new' between 'prev' and 'next'.
 * Maintains list integrity; callers must supply valid prev/next.
 */
static inline void __list_add(struct list_head *new, struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/* Add 'new' immediately after 'head' (insert at beginning). */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/* Add 'new' before 'head' (insert at tail). */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/*
 * Internal helper: unlink nodes between prev and next.
 * Does not touch the removed entry metadata.
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/* Remove entry from list - does not reinitialize node. */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = (struct list_head *)0;
	entry->prev = (struct list_head *)0;
}

/* Remove entry and reinitialize it to an empty list head. */
static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

/* Test whether a list is empty. */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/* Move an entry to another list (insert at head). */
static inline void list_move(struct list_head *entry, struct list_head *head)
{
	list_del(entry);
	list_add(entry, head);
}

/* Move an entry to another list (insert at tail). */
static inline void list_move_tail(struct list_head *entry,
				  struct list_head *head)
{
	list_del(entry);
	list_add_tail(entry, head);
}

/*
 * container_of: given pointer to member, return pointer to containing struct.
 * This macro mirrors the kernel's container_of semantics.
 */
#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/* offsetof implementation */
#define offsetof(type, member) ((size_t) &((type *)0)->member)

/* Get the struct for this entry. */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/* Get the first entry in a list. */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

/* Get the first entry in a list or NULL if the list is empty. */
/* member_offset is offsetof(type, member) */
static inline void *list_first_entry_or_null(struct list_head *head,
					     size_t member_offset)
{
	if (list_empty(head))
		return NULL;
	return (void *)((char *)head->next - member_offset);
}

/* Iterate over list of given type. 'pos' is a pointer to the containing type. */
#define list_for_each_entry(pos, head, member)                         \
	for (pos = list_entry((head)->next, typeof(*pos), member);   \
	     &pos->member != (head);                                 \
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/* Iterate over list safely against removal of list entry. 'n' is a temp. */
#define list_for_each_entry_safe(pos, n, head, member)                  \
	for (pos = list_entry((head)->next, typeof(*pos), member),    \
	    n = list_entry(pos->member.next, typeof(*pos), member);   \
	     &pos->member != (head);                                  \
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* Raw iterator over struct list_head nodes. */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#endif /* MISC_LIST_H */