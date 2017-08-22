#ifndef _TIMEACTION_H_INCLUDED_
#define _TIMEACTION_H_INCLUDED_

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

typedef void (*fpn_ox_timer_handler)(void* arg);

struct timer_mgr_s;

struct timer_mgr_s* ox_timer_mgr_new(int num);
void ox_timer_mgr_delete(struct timer_mgr_s* self);

void ox_timer_mgr_schedule(struct timer_mgr_s* self);
int ox_timer_mgr_add(struct timer_mgr_s* self, fpn_ox_timer_handler callback, int64_t delay, void* arg);
void ox_timer_mgr_del(struct timer_mgr_s* self, int id);
void* ox_timer_mgr_getarg(struct timer_mgr_s* self, int id);

#ifdef  __cplusplus
}
#endif

#endif
