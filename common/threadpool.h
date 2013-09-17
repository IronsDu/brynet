#ifndef _THREAD_POOL_H_INCLUDED_
#define _THREAD_POOL_H_INCLUDED_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdbool.h>

struct thread_pool_s;

typedef void (*thread_msg_fun_pt)(struct thread_pool_s* self, void* msg);

struct thread_pool_s* thread_pool_new(thread_msg_fun_pt callback, int thread_num, int msg_num);
void thread_pool_delete(struct thread_pool_s* self);

void thread_pool_start(struct thread_pool_s* self);
void thread_pool_stop(struct thread_pool_s* self);
void thread_pool_wait(struct thread_pool_s* self);

bool thread_pool_pushmsg(struct thread_pool_s* self, void* data);

#ifdef  __cplusplus
}
#endif

#endif
