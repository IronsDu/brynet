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

/*  构造协程返回其ID   */
int coroutine_new(struct schedule *, coroutine_func, void *ud);

/*  唤起被阻塞的协程    */
void coroutine_resume(struct schedule *, int id);

/*  获取协程的状态 */
int coroutine_status(struct schedule *, int id);

/*  获取当前运行的协程ID */
int coroutine_running(struct schedule *);

/*  挂起当前协程自身(返回主协程)    */
void coroutine_yield(struct schedule *);

/*  阻塞当前协程(返回主协程)   */
void coroutine_block(struct schedule *);

/*  协程调度器   */
void coroutine_schedule(struct schedule*);

#ifdef  __cplusplus
}
#endif

#endif
