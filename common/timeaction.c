#include <stdlib.h>
#include <string.h>

#include "heap.h"
#include "systemlib.h"
#include "stack.h"
#include "timeaction.h"

#define ERROR_COMPOSITOR_INDEX (-1)

struct timeaction_node_s
{
    int index;
    int64_t left_time;
    void*   arg;
    fpn_ox_timer_handler    callback;
};

static void 
timeaction_node_init(struct timeaction_node_s* self, int index)
{
    self->index = index;
    self->left_time = 0;
    self->arg = NULL;
    self->callback = NULL;
}

struct timer_mgr_s
{
    int node_num;

    struct stack_s*    free_node_ids;
    struct timeaction_node_s*   node_array;

    struct heap_s*  compositor_heap;
};

static void 
timeaction_mgr_destroy(struct timer_mgr_s* self)
{
    if(self->free_node_ids != NULL)
    {
        ox_stack_delete(self->free_node_ids);
        self->free_node_ids = NULL;
    }
    
    if(self->node_array != NULL)
    {
        free(self->node_array);
        self->node_array = NULL;
    }

    if(self->compositor_heap != NULL)
    {
        ox_heap_delete(self->compositor_heap);
        self->compositor_heap = NULL;
    }
}

static bool compare_timer(struct heap_s* self, const void* a, const void* b)
{
    int index_a = *(int*)a;
    int index_b = *(int*)b;
    struct timer_mgr_s* tm = (struct timer_mgr_s*)ox_heap_getext(self);

    struct timeaction_node_s* node_a = tm->node_array+index_a;
    struct timeaction_node_s* node_b = tm->node_array+index_b;

    return node_a->left_time < node_b->left_time;
}

static void swap_timer(struct heap_s* self, void* a, void* b)
{
    int temp = *(int*)a;
    *(int*)a = *(int*)b;
    *(int*)b = temp;
}

static bool
timeaction_mgr_init(struct timer_mgr_s* self, int num)
{
    struct timeaction_node_s* new_node_array = NULL;
    struct stack_s* new_freenode_ids = NULL;
    
    if(self->compositor_heap == NULL)
    {
        self->compositor_heap = ox_heap_new(num, sizeof(int), compare_timer, swap_timer, self);
    }
    else
    {
        if(!ox_heap_increase(self->compositor_heap, num))
        {
            return false;
        }
    }

    new_node_array = (struct timeaction_node_s*)malloc(num*sizeof(struct timeaction_node_s));
    new_freenode_ids = ox_stack_new(num, sizeof(int));

    if(new_node_array != NULL && new_freenode_ids != NULL)
    {
        const int old_nodenum = self->node_num;
        int i = old_nodenum;

        if(self->free_node_ids != NULL)
        {
            void* index_ptr = NULL;
            struct stack_s* old_freenode_ids = self->free_node_ids;

            while((index_ptr = ox_stack_popback(old_freenode_ids)) != NULL)
            {
                ox_stack_push(new_freenode_ids, index_ptr);
            }
        }

        if(self->node_array != NULL)
        {
            memcpy(new_node_array, self->node_array, (old_nodenum*sizeof(struct timeaction_node_s)));
        }
        
        i = old_nodenum;
        for(; i < num; ++i)
        {
            timeaction_node_init(new_node_array+i, i);
            ox_stack_push(new_freenode_ids, &i);
        }

        if(self->free_node_ids != NULL)
        {
          ox_stack_delete(self->free_node_ids);
          self->free_node_ids = NULL;
        }

        if(self->node_array != NULL)
        {
          free(self->node_array);
          self->node_array = NULL;
        }

        self->node_array = new_node_array;
        self->free_node_ids = new_freenode_ids;
        self->node_num = num;

        return true;
    }
    else
    {
        if(new_node_array != NULL)
        {
            free(new_node_array);
            new_node_array = NULL;
        }

        if(new_freenode_ids != NULL)
        {
            ox_stack_delete(new_freenode_ids);
            new_freenode_ids = NULL;
        }
    }

    return false;
}

struct timer_mgr_s* 
ox_timer_mgr_new(int num)
{
    struct timer_mgr_s* ret = (struct timer_mgr_s*)malloc(sizeof(struct timer_mgr_s));
    if(ret != NULL)
    {
        memset(ret, 0, sizeof(*ret));

        if(!timeaction_mgr_init(ret, num))
        {
            ox_timer_mgr_delete(ret);
            ret = NULL;
        }
    }

    return ret;
}

void 
ox_timer_mgr_delete(struct timer_mgr_s* self)
{
    if(self != NULL)
    {
        timeaction_mgr_destroy(self);
        free(self);
        self = NULL;
    }
}

void 
ox_timer_mgr_schedule(struct timer_mgr_s* self)
{
    int64_t now = ox_getnowtime();
    fpn_ox_timer_handler callback = NULL;
    void* arg = NULL;
    struct heap_s*  compositor_heap = self->compositor_heap;

    while(true)
    {
        int* nodepp = (int*)ox_heap_top(compositor_heap);
        struct timeaction_node_s* node = NULL;

        if(nodepp == NULL)
        {
            break;
        }

        node = self->node_array+*nodepp;
        callback = node->callback;

        if(callback == NULL)
        {
            ox_heap_pop(compositor_heap);
            ox_stack_push(self->free_node_ids, &node->index);
        }
        else
        {
            if(node->left_time > now)
            {
                break;
            }
            
            ox_heap_pop(compositor_heap);
            arg = node->arg;
            ox_stack_push(self->free_node_ids, &node->index);
            (callback)(arg);
        }
    }
}

int 
ox_timer_mgr_add(struct timer_mgr_s* self, fpn_ox_timer_handler callback, int64_t delay, void* arg)
{
    int id = ERROR_COMPOSITOR_INDEX;
    int* free_id = NULL;
    struct timeaction_node_s* node = NULL;

    if(callback == NULL)
    {
        return id;
    }
    
    free_id = (int*)ox_stack_popback(self->free_node_ids);
    if(free_id == NULL)
    {
        timeaction_mgr_init(self, 2*self->node_num);
        free_id = (int*)ox_stack_popback(self->free_node_ids);
    }

    if(free_id != NULL)
    {
        id = *free_id;
        node = self->node_array+id;
        node->callback = callback;
        node->arg = arg;
        if(delay > 0)
        {
            node->left_time = (delay+ox_getnowtime());
        }
        else
        {
            node->left_time = ox_getnowtime() - (-delay);
        }

        ox_heap_insert(self->compositor_heap, &(node->index));  
    }

    return id;
}

void 
ox_timer_mgr_del(struct timer_mgr_s* self, int id)
{
    if(id >= 0 && id < self->node_num)
    {
        self->node_array[id].callback = NULL;
    }
}

void*
ox_timer_mgr_getarg(struct timer_mgr_s* self, int id)
{
    if(id >= 0 && id < self->node_num)
    {
        return self->node_array[id].arg;
    }

    return NULL;
}