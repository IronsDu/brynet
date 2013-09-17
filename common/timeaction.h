#ifndef _TIMEACTION_H_INCLUDED_
#define _TIMEACTION_H_INCLUDED_

#include <stdint.h>
#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef void (*fpn_ox_timer_handler)(void* arg);

struct timer_mgr_s;

DLL_CONF    struct timer_mgr_s* ox_timer_mgr_new(int num);
DLL_CONF    void ox_timer_mgr_delete(struct timer_mgr_s* self);

DLL_CONF    void ox_timer_mgr_schedule(struct timer_mgr_s* self);
DLL_CONF    int ox_timer_mgr_add(struct timer_mgr_s* self, fpn_ox_timer_handler callback, int64_t delay, void* arg);
DLL_CONF    void ox_timer_mgr_del(struct timer_mgr_s* self, int id);
DLL_CONF    void* ox_timer_mgr_getarg(struct timer_mgr_s* self, int id);

#ifdef  __cplusplus
}
#endif

#endif
