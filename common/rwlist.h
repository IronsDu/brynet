#ifndef _RWLIST_H_INCLUDED_
#define _RWLIST_H_INCLUDED_

#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct rwlist_s;
struct stack_s;

DLL_CONF    struct rwlist_s* ox_rwlist_new(int num, int element_size, int max_pengding_num);
DLL_CONF    void ox_rwlist_delete(struct rwlist_s* self);
DLL_CONF    void ox_rwlist_push(struct rwlist_s* self, const void* data);

DLL_CONF    char*   ox_rwlist_front(struct rwlist_s* self, int timeout);
DLL_CONF    char*   ox_rwlist_pop(struct rwlist_s* self, int timeout);

DLL_CONF    void    ox_rwlist_flush(struct rwlist_s* self);
DLL_CONF    void    ox_rwlist_force_flush(struct rwlist_s* self);

DLL_CONF    bool    ox_rwlist_allempty(struct rwlist_s* self);

#ifdef  __cplusplus
}
#endif

#endif
