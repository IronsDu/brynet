#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "typepool.h"
#include "list.h"

struct list_s
{
    struct list_node_s head;
    struct list_node_s tail;

    struct type_pool_s* node_pool;
    struct type_pool_s* data_pool;

    int element_size;
};

struct list_s* 
ox_list_new(int node_num, int element_size)
{
    struct list_s* ret = (struct list_s*)malloc(sizeof(struct list_s));

    if(ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));

        ret->node_pool = ox_type_pool_new(node_num, sizeof(struct list_node_s));
        ret->data_pool = ox_type_pool_new(node_num, element_size);

        if(ret->node_pool != NULL && ret->data_pool != NULL)
        {
            ret->head.next = &(ret->tail);
            ret->tail.prior = &(ret->head);

            ret->element_size = element_size;
        }
        else
        {
            ox_list_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

void 
ox_list_delete(struct list_s* self)
{
    if(self != NULL)
    {
        if(self->node_pool != NULL)
        {
            ox_type_pool_delete(self->node_pool);
            self->node_pool = NULL;
        }
        
        if(self->data_pool != NULL)
        {
            ox_type_pool_delete(self->data_pool);
            self->data_pool = NULL;
        }

        self->element_size = 0;

        free(self);
        self = NULL;
    }
}

bool 
ox_list_push_back(struct list_s* self, const void* data)
{
    void* data_buffer = NULL;
    struct list_node_s* current_tail = NULL;
    struct list_node_s* node = (struct list_node_s*)ox_type_pool_claim(self->node_pool);

    if(node == NULL)
    {
        return false;
    }

    data_buffer = ox_type_pool_claim(self->data_pool);
    if(data_buffer == NULL)
    {
        ox_type_pool_reclaim(self->node_pool, (char*)node);
        return false;
    }

    node->data = data_buffer;
    memcpy(data_buffer, data, self->element_size);

    current_tail = self->tail.prior;
    current_tail->next = node;

    node->prior = current_tail;
    node->next = &(self->tail);
    self->tail.prior = node;
    
    return true;
}

struct list_node_s* 
ox_list_erase(struct list_s* self, struct list_node_s* node)
{
    struct list_node_s* prior = node->prior;
    struct list_node_s* next = node->next;

    assert(node != &self->head);
    assert(node != &self->tail);

    prior->next = next;
    next->prior = prior;

    ox_type_pool_reclaim(self->node_pool, (char*)node);
    ox_type_pool_reclaim(self->data_pool,(char*) node->data);
    return next;
}

struct list_node_s* 
ox_list_begin(struct list_s* self)
{
    return (self->head.next);
}

const struct list_node_s* 
ox_list_end(struct list_s* self)
{
    return &(self->tail);
}
