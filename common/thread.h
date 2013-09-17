#ifndef _THREAD_H_INCLUDED_
#define _THREAD_H_INCLUDED_

#include "dllconf.h"
#include "stdbool.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef void (*fpn_thread_fun)(void*);
struct thread_s;

DLL_CONF struct thread_s* ox_thread_new(fpn_thread_fun func, void* param);
DLL_CONF bool ox_thread_isrun(struct thread_s* self);
DLL_CONF void ox_thread_delete(struct thread_s* self);
DLL_CONF void ox_thread_wait(struct thread_s* self);
DLL_CONF void ox_thread_sleep(int milliseconds);

#ifdef  __cplusplus
}
#endif

#endif
