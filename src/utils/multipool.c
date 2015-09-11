#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "typepool.h"
#include "multipool.h"

struct multi_pool_basemsg_data_s
{
    char    pool_index;
};

struct multi_pool_s
{
    struct type_pool_s**    msg_pools;
    int                     max;
    int*                    lens;
};

struct multi_pool_s*
ox_multi_pool_new(const int* nums, const int* lens, int max, int add)
{
    struct multi_pool_s* ret = (struct multi_pool_s*)malloc(sizeof(*ret));

    ret->max = max;
    ret->lens = (int*)malloc(sizeof(int)*max);
    memcpy(ret->lens, lens, sizeof(int)*max);

    {
        int i = 0;
        for(; i < max; ++i)
        {
            ret->lens[i] += add;
        }
    }

    ret->msg_pools = (struct type_pool_s**)malloc(sizeof(struct type_pool_s*)*max);

    {
        int i = 0;
        for(; i < max; ++i)
        {
            ret->msg_pools[i] = ox_type_pool_new(nums[i], ret->lens[i]+sizeof(struct multi_pool_basemsg_data_s));
        }
    }

    return ret;
}

void
ox_multi_pool_delete(struct multi_pool_s* self)
{
    int i = 0;
    for(; i < self->max; ++i)
    {
        ox_type_pool_delete(self->msg_pools[i]);
    }

    free(self->msg_pools);

    free(self->lens);

    free(self);
}

char*
ox_multi_pool_claim(struct multi_pool_s* self, int pool_index)
{
    char* ret = NULL;
    
    assert(pool_index >= 0 && pool_index < self->max);

    if(pool_index >= 0 && pool_index < self->max)
    {
        ret = ox_type_pool_claim(self->msg_pools[pool_index]);
        if(ret != NULL)
        {
            ((struct multi_pool_basemsg_data_s*)ret)->pool_index = pool_index;

            ret += sizeof(struct multi_pool_basemsg_data_s);
        }
    }

    return ret;
}

char*
ox_multi_pool_lenclaim(struct multi_pool_s* self, int len)
{
    char* ret = NULL;
    int pool_index = 0;
    char packet_pool_max = self->max;
    int* packet_pool_len = self->lens;

    for(;pool_index < packet_pool_max; ++pool_index)
    {
        if(packet_pool_len[pool_index] >= len)
        {
            break;
        }
    }

    if(pool_index < packet_pool_max)
    {
        ret = ox_type_pool_claim(self->msg_pools[pool_index]);
        if(ret != NULL)
        {
            ((struct multi_pool_basemsg_data_s*)ret)->pool_index = pool_index;
            ret += sizeof(struct multi_pool_basemsg_data_s);
        }
    }

    return ret;
}

void
ox_multi_pool_reclaim(struct multi_pool_s* self, char* msg)
{
    msg -= sizeof(struct multi_pool_basemsg_data_s);

    {
        struct multi_pool_basemsg_data_s* base_msg = (struct multi_pool_basemsg_data_s*)msg;
        ox_type_pool_reclaim(self->msg_pools[(int)base_msg->pool_index], (char*)msg);
    }
}

int
ox_multi_pool_config_len(struct multi_pool_s* self, const char* msg)
{
    int len = -1;
    msg -= sizeof(struct multi_pool_basemsg_data_s);
    {
        const struct multi_pool_basemsg_data_s* base_msg = (struct multi_pool_basemsg_data_s*)msg;
        int pool_index = base_msg->pool_index;

        assert(pool_index >= 0 && pool_index < self->max);

        if(pool_index >= 0 && pool_index < self->max)
        {
            len = self->lens[pool_index];
        }
    }

    return len;
}

int
ox_multi_pool_typelen(struct multi_pool_s* self, int pool_index)
{
    int ret = 0;
    if(pool_index >= 0 && pool_index < self->max)
    {
        ret = self->lens[pool_index];
    }

    return ret;
}

int
ox_multi_pool_nodenum(struct multi_pool_s* self)
{
    int ret = 0;
    int i = 0;
    for(; i < self->max; ++i)
    {
        ret += ox_type_pool_nodenum(self->msg_pools[i]);
    }

    return ret;
}