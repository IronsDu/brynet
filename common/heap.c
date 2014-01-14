#include <stdlib.h>
#include <string.h>
#include "array.h"

#include "heap.h"

struct heap_s
{
    struct array_s* data;
    int num;                /*  当前堆中有效元素个数    */

    pfn_compare compare;
    pfn_swap swap;
    void* ext;
};

struct heap_s* 
ox_heap_new(int element_num, int element_size, pfn_compare compare, pfn_swap swap, void* ext)
{
    struct heap_s* ret = (struct heap_s*)malloc(sizeof(*ret));
    if(ret != NULL)
    {
        ret->data = ox_array_new(element_num, element_size);
        ret->num = 0;
        ret->compare = compare;
        ret->swap = swap;
        ret->ext = ext;
    }

    return ret;
}

void 
ox_heap_delete(struct heap_s* self)
{
    if(self != NULL)
    {
        ox_array_delete(self->data);
        self->data = NULL;

        free(self);
        self = NULL;
    }
}

static int 
parent(int index)
{
    return (index-1)/2;
}

static int 
left(int index)
{
    return 2*index+1;
}

static int 
right(int index)
{
    return 2*index+2;
}

static void 
up(struct heap_s* self, int index)
{
    int parent_index = parent(index);

    while(parent_index >= 0 && index != parent_index)
    {
        void* index_data = ox_array_at(self->data, index);
        void* parent_data = ox_array_at(self->data, parent_index);

        if((self->compare)(self, index_data, parent_data))
        {
            (self->swap)(self, index_data, parent_data);

            index = parent_index;
            parent_index = parent(index);
        }
        else
        {
            break;
        }
    }
}

static void
down(struct heap_s* self, int index)
{
    int l = left(index);
    int r = right(index);
    int min = index;

    void* index_value = ox_array_at(self->data, index);
    void* min_value = index_value;

    if(l < self->num)
    {
        void* l_value = ox_array_at(self->data, l);
        if((self->compare)(self, l_value, index_value))
        {
            min = l;
            min_value = l_value;
        }
    }

    if(r < self->num)
    {
        void* r_value = ox_array_at(self->data, r);
        if((self->compare)(self, r_value, min_value))
        {
            min = r;
            min_value = r_value;
        }
    }

    if(min != index)
    {
        (self->swap)(self, index_value, min_value);
        down(self, min);
    }
}

bool 
ox_heap_insert(struct heap_s* self, const void* datap)
{
    if(ox_array_num(self->data) <= self->num)  /*  如果数据空间不足,则增长heap */
    {
        ox_heap_increase(self, self->num);
    }

    if(ox_array_set(self->data, self->num, datap))
    {
        up(self, self->num);
        self->num++;

        return true;
    }
    else
    {
        return false;
    }
}

void* ox_heap_top(struct heap_s* self)
{
    return (self->num > 0) ? ox_array_at(self->data, 0) : NULL;
}

void* 
ox_heap_pop(struct heap_s* self)
{
    if(self->num > 0)
    {
        void* min = ox_array_at(self->data, 0);
        void* end = ox_array_at(self->data, self->num-1);

        if(min != end)
        {
            (self->swap)(self, min, end);
        }

        self->num--;
        down(self, 0);

        return end;
    }
    else
    {
        return NULL;
    }
}

void 
ox_heap_clear(struct heap_s* self)
{
    self->num = 0;
}

bool 
ox_heap_increase(struct heap_s* self, int incnum)
{
    return ox_array_increase(self->data, incnum);
}

void*
ox_heap_getext(struct heap_s* self)
{
    return self->ext;
}