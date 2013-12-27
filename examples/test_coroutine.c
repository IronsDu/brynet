#include "coroutine.h"
#include <stdio.h>
#include <time.h>

struct args {
	int n;
};

static void
foo(struct schedule * S, void *ud) {
	struct args * arg = (struct args *)ud;
	int start = arg->n;
    int now_time = time(NULL);
	int i;
    printf("foo start \n");
	for (i=0;i<20000000;i++) {
		//printf("coroutine %d : %d\n",coroutine_running(S) , start + i);
		coroutine_yield(S);
	}

    printf("cost : %d \n", time(NULL) - now_time);
}

static void
test(struct schedule *S) {
	struct args arg1 = { 0 };
	struct args arg2 = { 100 };

	coroutine_new(S, foo, &arg1);
    coroutine_new(S, foo, &arg2);
    coroutine_new(S, foo, &arg2);

	printf("main start\n");
	while (1) {
		coroutine_schedule(S);
	} 
	printf("main end\n");
}

#include <assert.h>

int 
main() {
	struct schedule * S = coroutine_open();
	test(S);
	coroutine_close(S);
	getchar();
	return 0;
}

