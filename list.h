#ifndef _LIST_H_
#define _LIST_H_
#include <sys/cdefs.h>
#include <stddef.h>

namespace common {
	namespace base {



__BEGIN_DECLS

#define __builtin_prefetch(x, y, z) (void)1

struct list_head{
	struct list_head *next, *prev;
};

typedef struct list_head list_head_t;


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


static inline void __list_add(struct list_head *p,
							  struct list_head *prev,
							  struct list_head *next)
{
	next->prev = p;
	p->next = next;
	p->prev = prev;
	prev->next = p;
}

static inline void list_add(struct list_head *p, struct list_head *head)
{
	__list_add(p, head, head->next);
}

static inline void list_add_tail(struct list_head *p, struct list_head *head)
{
	__list_add(p, head->prev, head);
}


static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = 0;
	entry->prev = 0;
}

/*
 * delete entry from old list and reinit it 
 */
static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

/*
 * delete from one list and add as another's head
 */ 
static inline void list_move(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

/*
 * delete from one list and add as another's tail
 */ 
static inline void list_move_tail(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add_tail(list, head);
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/*
 *
 */ 
static inline void __list_splice(struct list_head *list,
								 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/*
 * join two lists
 */ 
static inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head);
}

static inline void list_splice_init(struct list_head *list,
								    struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head);
		INIT_LIST_HEAD(list);
	}
}


#ifndef offsetof

#if __GUNC__ >= 4
#define offsetof(type, member) __builtin_offsetof(type, member)
#else
#define offsetof(type, member) (unsigned long)(&((type *)0)->member)
#endif

#endif


/**
 * get the struct for this entry
 *
 * @ptr: the &strcut list_head pointer
 * @tyoe: the type of the struct this is embedded in
 * @member: the name of the list_struct within the struct 
 */ 
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * iterate over a list
 * @pos: the &struct list_head to use as a loop counter
 * @head: the head for your list.
 */ 
#define list_for_each(pos, head) \
	for (pos = (head)->next, __builtin_prefetch(pos->next, 0, 1); \
			pos != (head); \
			pos = pos->next, __builtin_prefetch(pos->next, 0, 1)) \

#define __list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)


#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev, __builtin_prefetch(pos->prev, 0, 1); \
			pos != (head); \
			pos = pos->prev, __builtin_prefetch(pos->prev, 0, 1)) \

#define __list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)


/**
 * iterate over list of given type
 *
 * @pos: the type * to use as a loop counter
 * @head: the head for your list
 * @member: the name of the list_struct within the struct
 */ 
#define list_for_each_entry(pos, head, member) \
	for (pos = list_entry((head)->next, typeof(*pos), member), \
			__builtin_prefetch(pos->member.next, 0, 1); \
			&pos->member != (head); \
			pos = list_entry(pos->member.next, typeof(*pos), member), \
			__builtin_prefetch(pos->member.next, 0, 1))



__END_DECLS


class CListHead
{
	public:
		struct list_head objlist;

		void InitList() {
			INIT_LIST_HEAD(&objlist);
		}

		void ResetList() {
			list_del_init(&objlist);
		}

		int ListEmpty() const {
			return list_empty(&objlist);
		}

		CListHead *ListNext(){
			return list_entry(objlist.next, CListHead, objlist);
		}

		CListHead *ListPrev(){
			return list_entry(objlist.prev, CListHead, objlist);
		}

		void ListAdd(CListHead &n) {
			list_add(&objlist, &n.objlist);
		}

		void ListAdd(CListHead *n) {
			list_add(&objlist, &n->objlist);
		}

		void ListAddTail(CListHead &n) {
			list_add_tail(&objlist, &n.objlist);
		}

		void ListAddTail(CListHead *n) {
			list_add_tail(&objlist, &n->objlist);
		}

		void ListDel() {
			ResetList();
		}

		void ListMove(CListHead &n) {
			list_move(&objlist, &n.objlist);
		}

		void ListMove(CListHead *n) {
			list_move(&objlist, &n->objlist);
		}

		void ListMoveTail(CListHead &n) {
			list_move_tail(&objlist, &n.objlist);
		}

		void ListMoveTail(CListHead *n) {
			list_move_tail(&objlist, &n->objlist);
		}

		void FreeList() {
			while (!ListEmpty())
				ListNext()->ResetList();
		}
};

template<class T>
class CListObject: public CListHead
{
	public:
		CListObject() {
			InitList();
		}

		~CListObject() {
			ResetList();
		}

		CListObject<T> *ListNext() {
			return (CListObject<T> *)CListHead::ListNext();
		}

		CListObject<T> *ListPrev() {
			return (CListObject<T> *)CListHead::ListPrev();
		}

		T *ListOwner() {
			return (T *)this;
		}

		T *NextOwner(){
			return ListNext()->ListOwner();
		}

		T *PrevOwner(){
			return ListPrev()->ListOwner();
		}
};



	}
}

#endif
