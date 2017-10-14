#ifndef C_COROUTINE_H
#define C_COROUTINE_H

#ifdef  __cplusplus
extern "C" {
#endif

enum COROUTINE_STATE
{
    COROUTINE_READY,
    COROUTINE_RUNNING,
    COROUTINE_SUSPEND,
    COROUTINE_BLOCK,
    COROUTINE_DEAD,
};

struct schedule;

typedef void (*coroutine_func)(struct schedule *, void *ud);

struct schedule * coroutine_open(void);
void coroutine_close(struct schedule *);

int coroutine_new(struct schedule *, coroutine_func, void *ud);

void coroutine_resume(struct schedule *, int id);

int coroutine_status(struct schedule *, int id);

int coroutine_running(struct schedule *);

void coroutine_yield(struct schedule *);

void coroutine_block(struct schedule *);

void coroutine_schedule(struct schedule*);

#ifdef  __cplusplus
}
#endif

#endif
