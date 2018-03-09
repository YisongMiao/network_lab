#ifndef __HASH_LIST_H__
#define __HASH_LIST_H__

struct hash_list_node {
	struct hash_list_node *next;
	struct hash_list_node **pprev;
};

struct hash_list_head {
	struct hash_list_node *head;
};

static inline int hash_list_unhashed(struct hash_list_node *node)
{
	return ! node->pprev;
}

static inline int hash_list_empty(struct hash_list_head *list)
{
	return ! list->head;
}

static inline void init_hash_list_head(struct hash_list_head *list)
{
	list->head = NULL;
}

static inline void init_hash_list_node(struct hash_list_node *node)
{
	node->next = NULL;
	node->pprev = NULL;
}

static inline void hash_list_add_head(struct hash_list_node *node, \
		struct hash_list_head *list)
{
	node->next = list->head;
	node->pprev = &list->head;
	if (list->head)
		list->head->pprev = &node->next;
	list->head = node;
}

static inline void hash_list_add_before(struct hash_list_node *node, \
		struct hash_list_node *next)
{
	node->next = next;
	node->pprev = next->pprev;
	*next->pprev = node;
	next->pprev = &node->next;
}

static inline void hash_list_add_after(struct hash_list_node *node, \
		struct hash_list_node *prev)
{
	node->next = prev->next;
	node->pprev = &prev->next;
	if (prev->next)
		prev->next->pprev = &node->next;
	prev->next = node;
}

static inline void hash_list_delete_node(struct hash_list_node *node)
{
	*node->pprev = node->next;
	if (node->next)
		node->next->pprev = node->pprev;

	node->next = NULL;
	node->pprev = NULL;
}

#define hash_list_entry(ptr, type, member) \
		((ptr) ? (type *)((char *)ptr - offsetof(type, member)) : NULL)

#define hash_list_for_each_entry(pos, list, member) \
	for (pos = hash_list_entry((list)->head, typeof(*pos), member); pos; \
			pos = hash_list_entry(pos->member.next, typeof(*pos), member))

#endif
