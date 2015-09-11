#include "coroutine.h"
#ifndef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "stack.h"

#define STACK_SIZE (1024*1024)
#define DEFAULT_COROUTINE 16

struct coroutine;

struct schedule {
	char stack[STACK_SIZE];
	ucontext_t main;
	int nco;
	int cap;
	int running;
	struct coroutine **cos;
    struct stack_s* active_list;
};

struct coroutine {
	coroutine_func func;
	void *ud;
	ucontext_t ctx;
	struct schedule * sch;
	ptrdiff_t cap;
	ptrdiff_t size;
	int state;
	char *stack;
};

struct coroutine * 
_co_new(struct schedule *S , coroutine_func func, void *ud) {
	struct coroutine * co = malloc(sizeof(*co));
	co->func = func;
	co->ud = ud;
	co->sch = S;
	co->cap = 0;
	co->size = 0;
	co->state = COROUTINE_READY;
	co->stack = NULL;
	return co;
}

void
_co_delete(struct coroutine *co) {
	free(co->stack);
	free(co);
}

struct schedule * 
coroutine_open(void) {
	struct schedule *S = malloc(sizeof(*S));
	S->nco = 0;
	S->cap = DEFAULT_COROUTINE;
	S->running = -1;
	S->cos = malloc(sizeof(struct coroutine *) * S->cap);
	memset(S->cos, 0, sizeof(struct coroutine *) * S->cap);
	S->active_list = ox_stack_new(1024, sizeof(int));
	return S;
}

void 
coroutine_close(struct schedule *S) {
	int i;
	for (i=0;i<S->cap;i++) {
		struct coroutine * co = S->cos[i];
		if (co) {
			_co_delete(co);
		}
	}
	free(S->cos);
	S->cos = NULL;
	free(S);
}

int 
coroutine_new(struct schedule *S, coroutine_func func, void *ud) {
	struct coroutine *co = _co_new(S, func , ud);
	if (S->nco >= S->cap) {
		int id = S->cap;
		S->cos = realloc(S->cos, S->cap * 2 * sizeof(struct coroutine *));
		memset(S->cos + S->cap , 0 , sizeof(struct coroutine *) * S->cap);
		S->cos[S->cap] = co;
		S->cap *= 2;
		++S->nco;
		ox_stack_push(S->active_list, &id);
		return id;
	} else {
		int i;
		for (i=0;i<S->cap;i++) {
			int id = (i+S->nco) % S->cap;
			if (S->cos[id] == NULL) {
				S->cos[id] = co;
				++S->nco;
				ox_stack_push(S->active_list, &id);
				return id;
			}
		}
	}
	assert(0);
	return -1;
}

static void
mainfunc(uint32_t low32, uint32_t hi32) {
	uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
	struct schedule *S = (struct schedule *)ptr;
	int id = S->running;
	struct coroutine *C = S->cos[id];
	C->func(S,C->ud);
	_co_delete(C);
	S->cos[id] = NULL;
	--S->nco;
	S->running = -1;
}

static void
coroutine_goto(struct schedule * S, int id) {
    assert(S->running == -1);
    assert(id >=0 && id < S->cap);
    struct coroutine *C = S->cos[id];
    if (C == NULL)
        return;
    int state = C->state;
    switch(state) {
    case COROUTINE_READY:
        getcontext(&C->ctx);
        C->ctx.uc_stack.ss_sp = S->stack;
        C->ctx.uc_stack.ss_size = STACK_SIZE;
        C->ctx.uc_link = &S->main;
        S->running = id;
        C->state = COROUTINE_RUNNING;
        uintptr_t ptr = (uintptr_t)S;
        makecontext(&C->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));
        swapcontext(&S->main, &C->ctx);
        break;
    case COROUTINE_SUSPEND:
        memcpy(S->stack + STACK_SIZE - C->size, C->stack, C->size);
        S->running = id;
        C->state = COROUTINE_RUNNING;
        swapcontext(&S->main, &C->ctx);
        break;
    default:
        assert(0);
    }
}

static void
_save_stack(struct coroutine *C, char *top) {
	char dummy = 0;
	assert(top - &dummy <= STACK_SIZE);
	if (C->cap < top - &dummy) {
		free(C->stack);
		C->cap = top-&dummy;
		C->stack = malloc(C->cap);
	}
	C->size = top - &dummy;
	memcpy(C->stack, &dummy, C->size);
}

void
coroutine_yield(struct schedule * S) {
	int id = S->running;
	assert(id >= 0);
	struct coroutine * C = S->cos[id];
	assert((char *)&C > S->stack);
	_save_stack(C,S->stack + STACK_SIZE);
	C->state = COROUTINE_SUSPEND;
        ox_stack_push(S->active_list, &id);
	S->running = -1;
	swapcontext(&C->ctx , &S->main);
}

void
coroutine_block(struct schedule * S) {
    int id = S->running;
    assert(id >= 0);
    struct coroutine * C = S->cos[id];
    assert((char *)&C > S->stack);
    _save_stack(C,S->stack + STACK_SIZE);
    C->state = COROUTINE_BLOCK;
    S->running = -1;
    swapcontext(&C->ctx , &S->main);
}

int 
coroutine_status(struct schedule * S, int id) {
	assert(id>=0 && id < S->cap);
	if (S->cos[id] == NULL) {
		return COROUTINE_DEAD;
	}
	return S->cos[id]->state;
}

int 
coroutine_running(struct schedule * S) {
	return S->running;
}

#else

#include <assert.h>
#include <stdlib.h>
#include <windows.h>

#include "array.h"
#include "stack.h"

struct coroutine;

struct schedule {
    LPVOID  main;
    struct array_s* cos;

    /*  空闲ID  */
    struct stack_s* freeids;

    struct stack_s* active_list;

    int current;        /*  当前运行的协程ID   */
};

struct coroutine {
    int state;
    LPVOID  fiber;
};

static void increase_coroutine(struct schedule* s, int num) {
    int old_num = 0;
    int new_num = 0;
    if(s->cos == NULL) {
        s->freeids = ox_stack_new(num, sizeof(int));
        s->cos = ox_array_new(num, sizeof(struct coroutine*));
    } else {
        old_num = ox_array_num(s->cos);
        ox_array_increase(s->cos, num);
    }

    new_num = ox_array_num(s->cos);

    {
        int i = old_num;
        struct coroutine* tmp = NULL;
        for(; i < new_num; ++i) {
            ox_array_set(s->cos, i, &tmp);
            ox_stack_push(s->freeids, &i);
        }
    }
}

struct schedule * coroutine_open(void) {
    struct schedule* ret = (struct schedule*)malloc(sizeof(*ret));
    ret->freeids = NULL;
    ret->cos = NULL;
    ret->active_list = ox_stack_new(1024, sizeof(int));

    increase_coroutine(ret, 1024);

    ret->main = ConvertThreadToFiber(NULL);

    ret->current = -1;

    return ret;
}

void coroutine_close(struct schedule * s) {

}

static
int claim_coroutine_id(struct schedule * s) {
    int *ret = NULL;
    if(ox_stack_num(s->freeids) == 0) {
        increase_coroutine(s, 1024);
    }

    ret = (int*)ox_stack_popback(s->freeids);
    return *ret;
}

struct coroutine_warp_para
{
    struct schedule * s;
    coroutine_func f;
    void*   ud;
};

VOID
    __stdcall
    coroutine_main(
    LPVOID lpParameter
    )
{
    struct coroutine_warp_para* para = (struct coroutine_warp_para*)lpParameter;
    struct schedule * s = para->s;

    (para->f)(para->s, para->ud);

    {
        struct coroutine* tmp = NULL;
        free(*(struct coroutine**)ox_array_at(s->cos, s->current));
        ox_array_set(s->cos, s->current, &tmp);
        s->current = -1;
        free(para);
    }

    /*  回到主协程   */
    SwitchToFiber(s->main);
}

int coroutine_new(struct schedule * s, coroutine_func f, void *ud) {
    int id = claim_coroutine_id(s);
    struct coroutine* co = (struct coroutine*)malloc(sizeof(*co));
    struct coroutine_warp_para* para = (struct coroutine_warp_para*)malloc(sizeof(*para));

    para->s = s;
    para->f = f;
    para->ud = ud;
    
    co->fiber = CreateFiber(0, coroutine_main, para);
    co->state = COROUTINE_READY;

    ox_array_set(s->cos, id, &co);

    ox_stack_push(s->active_list, &id);

    return id;
}

static void
coroutine_goto(struct schedule * s, int id) {
    struct coroutine* co = *(struct coroutine**)(ox_array_at(s->cos, id));
    assert(co != NULL && (co->state == COROUTINE_READY || co->state == COROUTINE_SUSPEND));
    if(co != NULL && (co->state == COROUTINE_READY || co->state == COROUTINE_SUSPEND)) {
        co->state = COROUTINE_RUNNING;
        s->current = id;
        SwitchToFiber(co->fiber);
    }
}

int coroutine_status(struct schedule * s, int id) {
    struct coroutine* co = *(struct coroutine**)(ox_array_at(s->cos, id));
    if(co != NULL) {
        return co->state;
    } else {
        return COROUTINE_DEAD;
    }
}

int coroutine_running(struct schedule * s) {
    return s->current;
}

void coroutine_yield(struct schedule * s) {
    int current = s->current;
    struct coroutine* co = *(struct coroutine**)(ox_array_at(s->cos, current));
    co->state = COROUTINE_SUSPEND;
    /*  暂时挂起，放入活动列表末尾   */
    ox_stack_push(s->active_list, &current);
    s->current = -1;

    SwitchToFiber(s->main);
}

void coroutine_block(struct schedule * s) {
    struct coroutine* co = *(struct coroutine**)(ox_array_at(s->cos, s->current));
    co->state = COROUTINE_BLOCK;

    s->current = -1;
    SwitchToFiber(s->main);
}

#endif

void coroutine_resume(struct schedule * s, int id) {
    struct coroutine* co = *(struct coroutine**)(ox_array_at(s->cos, id));
    if(co != NULL && co->state == COROUTINE_BLOCK) {
        /*  激活某协程,放入活动列表    */
        co->state = COROUTINE_SUSPEND;
        ox_stack_push(s->active_list, &id);
    }
}

void coroutine_schedule(struct schedule* s) {
    if(ox_stack_num(s->active_list) > 0) {
        int id = *(int*)ox_stack_popfront(s->active_list);
        coroutine_goto(s, id);
    }
}
