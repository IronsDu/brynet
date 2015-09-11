#ifndef _MULTI_POOL_H
#define _MULTI_POOL_H

#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct multi_pool_s;

DLL_CONF    struct multi_pool_s* ox_multi_pool_new(const int* nums, const int* lens, int max, int add);
DLL_CONF    void ox_multi_pool_delete(struct multi_pool_s* self);
DLL_CONF    char* ox_multi_pool_claim(struct multi_pool_s* self, int pool_index);
DLL_CONF    char* ox_multi_pool_lenclaim(struct multi_pool_s* self, int len);
DLL_CONF    void ox_multi_pool_reclaim(struct multi_pool_s* self, char* msg);
DLL_CONF    int ox_multi_pool_config_len(struct multi_pool_s* self, const char* msg);
DLL_CONF    int ox_multi_pool_typelen(struct multi_pool_s* self, int pool_index);
DLL_CONF    int ox_multi_pool_nodenum(struct multi_pool_s* self);

#ifdef  __cplusplus
}
#endif

#endif
