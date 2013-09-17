#ifndef _MUTEX_H_INCLUDED_
#define _MUTEX_H_INCLUDED_

#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct mutex_s;
struct thread_cond_s;
struct thread_spinlock_s;

DLL_CONF struct mutex_s* ox_mutex_new(void);
DLL_CONF void ox_mutex_delete(struct mutex_s* self);
DLL_CONF void ox_mutex_lock(struct mutex_s* self);
DLL_CONF void ox_mutex_unlock(struct mutex_s* self);

DLL_CONF struct thread_cond_s* ox_thread_cond_new(void);
DLL_CONF void ox_thread_cond_delete(struct thread_cond_s* self);
DLL_CONF void ox_thread_cond_wait(struct thread_cond_s* self, struct mutex_s* mutex);
DLL_CONF void ox_thread_cond_timewait(struct thread_cond_s* self, struct mutex_s* mutex, unsigned int timeout);
DLL_CONF void ox_thread_cond_signal(struct thread_cond_s* self);

DLL_CONF struct thread_spinlock_s* ox_thread_spinlock_new(void);
DLL_CONF void ox_thread_spinlock_delete(struct thread_spinlock_s* self);
DLL_CONF void ox_thread_spinlock_lock(struct thread_spinlock_s* self);
DLL_CONF void ox_thread_spinlock_unlock(struct thread_spinlock_s* self);

#ifdef  __cplusplus
}
#endif

#endif
