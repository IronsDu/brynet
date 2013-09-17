#ifndef _DOUBLE_LINK_H
#define _DOUBLE_LINK_H

#include <assert.h>

struct double_link_node_s
{
    struct double_link_node_s* prior;
    struct double_link_node_s* next;
};

struct double_link_s
{
    struct double_link_node_s  head;
    struct double_link_node_s  tail;
};

static inline void double_link_init(struct double_link_s* list)
{
    list->head.prior = NULL;
    list->tail.next = NULL;

    list->head.next = &list->tail;
    list->tail.prior = &list->head;
}

static inline void double_link_push_back(struct double_link_s* list, struct double_link_node_s* node)
{
    if(node->next == NULL && node->prior == NULL)
    {
        struct double_link_node_s* current_tail = list->tail.prior;
        current_tail->next = node;

        node->prior = current_tail;
        node->next = &list->tail;

        list->tail.prior = node;
    }
}

static inline void double_link_erase(struct double_link_s* list, struct double_link_node_s* node)
{
    struct double_link_node_s* prior = node->prior;
    struct double_link_node_s* next = node->next;

    if(prior != NULL && next != NULL)
    {
        assert(node != &list->head);
        assert(node != &list->tail);

        prior->next = next;
        next->prior = prior;

        node->next = NULL;
        node->prior = NULL;
    }
}

static inline struct double_link_node_s* double_link_begin(struct double_link_s* list)
{
    return list->head.next;
}

static inline struct double_link_node_s* double_link_end(struct double_link_s* list)
{
    return &list->tail;
}

#endif
