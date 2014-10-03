#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "array.h"
#include "stack.h"
#include "typepool.h"

struct type_pool_node_s
{
    struct array_s* data_array;
    struct stack_s* ptr_stack;
    struct type_pool_node_s* next;
};

struct type_pool_s
{
    int base_num;
    int element_size;
    struct type_pool_node_s* free_node;
    struct type_pool_node_s* head;
    struct type_pool_node_s* tail;

    int64_t                 nodenum;
};

static void 
type_pool_node_delete(struct type_pool_node_s* self)
{
    if(self != NULL)
    {
        if(self->data_array != NULL)
        {
            ox_array_delete(self->data_array);
            self->data_array = NULL;
        }

        if(self->ptr_stack != NULL)
        {
            ox_stack_delete(self->ptr_stack);
            self->ptr_stack = NULL;
        }

        self->next = NULL;
        free(self);
        self = NULL;
    }
}

static struct type_pool_node_s* 
type_pool_node_new(int num, int element_size)
{
    struct type_pool_node_s* ret = (struct type_pool_node_s*)malloc(sizeof(struct type_pool_node_s));

    if(ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));

        if( (ret->ptr_stack = ox_stack_new(num, sizeof(void*))) != NULL && 
            (ret->data_array = ox_array_new(num, element_size)) != NULL)
        {
            int i = 0;

            for(; i < num; ++i)
            {
                char* element_data = ox_array_at(ret->data_array, i);
                memset(element_data, 0, element_size);
                memcpy(element_data, &ret, sizeof(struct type_pool_node_s*));
                ox_stack_push(ret->ptr_stack, &element_data);
            }

            ret->next = NULL;
        }
        else
        {
            type_pool_node_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

struct type_pool_s* 
ox_type_pool_new(int num, int element_size)
{
    struct type_pool_s* ret = (struct type_pool_s*)malloc(sizeof(struct type_pool_s));

    if(ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));
        ret->nodenum = 0;
        ret->head = type_pool_node_new(num, element_size+sizeof(struct type_pool_node_s*));
        ret->nodenum++;
        if(ret->head != NULL)
        {
            ret->free_node = ret->tail = ret->head;
            ret->element_size = element_size;
            ret->base_num = num;
        }
        else
        {
            ox_type_pool_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

void 
ox_type_pool_delete(struct type_pool_s* self)
{
    if(self != NULL)
    {
        struct type_pool_node_s* node = self->head;
        while(node != NULL)
        {
            struct type_pool_node_s* node_temp = node;
            node = node_temp->next;
            type_pool_node_delete(node_temp);
        }

        self->head = self->tail = self->free_node = NULL;
        free(self);
        self = NULL;
    }
}

char*
type_pool_claim_help(struct type_pool_s* self)
{
    char* ret = NULL;
    struct type_pool_node_s* node = self->free_node;

    if(node != NULL)
    {
        ret = ox_stack_popback(node->ptr_stack);
    }

    if(ret == NULL)
    {
        node = self->head;
        while(ret == NULL && node != NULL)
        {
            ret = ox_stack_popback(node->ptr_stack);
            node = node->next;
        }
    }

    if(ret != NULL)
    {
        self->free_node = node;
        ret = *(char**)ret;
        ret += sizeof(struct type_pool_node_s*);
    }

    return ret;
}

char* 
ox_type_pool_claim(struct type_pool_s* self)
{
    char* ret = type_pool_claim_help(self);
    if(ret == NULL)
    {
        ox_type_pool_increase(self, self->base_num);
        self->nodenum++;
        ret = type_pool_claim_help(self);
    }
    
    return ret;
}

void 
ox_type_pool_reclaim(struct type_pool_s* self, char* data)
{
    struct type_pool_node_s* node = NULL;
    data -= sizeof(struct type_pool_node_s*);
    node = *(struct type_pool_node_s**)(data);
    ox_stack_push(node->ptr_stack, &data);
    self->free_node = node;
}

void 
ox_type_pool_increase(struct type_pool_s* self, int increase_num)
{
    struct type_pool_node_s* node = type_pool_node_new(increase_num, self->element_size+sizeof(struct type_pool_node_s*));
    
    if(node != NULL)
    {
        self->tail->next = node;
        self->tail = node;
        self->free_node = node;
    }
}

int
ox_type_pool_nodenum(struct type_pool_s* self)
{
    return self->nodenum;
}
