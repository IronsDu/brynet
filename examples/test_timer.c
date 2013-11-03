#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "typepool.h"
#include "systemlib.h"
#include "timeaction.h"
#include "heap.h"

static int count = 0;
static void my_handler(void* arg)
{
    printf("handler\n");
	count++;
}

static void my_handler1(void* arg)
{
  printf("handler1\n");
}

void typepool_cpu()
{
    struct type_pool_s* tp = ox_type_pool_new(40000, sizeof(int));
    int i = 0;
	unsigned int old =  ox_getnowtime();
    printf("开始分配：%d\n", old);
    while( i < 80001)
    {
		char* p = ox_type_pool_claim(tp);
		ox_type_pool_reclaim(tp, p);
        i++;
        ;
    }
    
    i = 0;
    while( i < 80000)
    {
        char* p = ox_type_pool_claim(tp);

        i++;
        ;
    }
    
    printf("分配完毕：%d\n", old);
}

void test_timer_cpu()
{
    unsigned int old =  ox_getnowtime();
    int id = -1;
    int i = 0;
    struct timer_mgr_s* time_mgr = 0;
    printf("开始构造:%d\n", old);
	time_mgr = ox_timer_mgr_new(10000000);
	id = ox_timer_mgr_add(time_mgr, my_handler1, 3000, NULL);

    old =  ox_getnowtime();
    printf("开始添加:%d\n", old);
    for(; i < 1000000; ++i)
    {
        id = ox_timer_mgr_add(time_mgr, my_handler, rand() % 2000, NULL);
    }
    old =  ox_getnowtime();
    //timeaction_mgr_del(time_mgr, id);
    printf("开始调度:%d\n", old);
    
	while(1)
	{
		ox_timer_mgr_schedule(time_mgr);
		if(count >= 1000000)
		{
			break;
		}
	}
    
    old =  ox_getnowtime();
    //timeaction_mgr_del(time_mgr, id);
    printf("调度结束:%d\n", old);   
}

bool compare_int(struct heap_s* heap, const void* a, const void* b)
{
  int aint = *(int*)a;
  int bint = *(int*)b;

  return aint > bint;
}


void swap_int(struct heap_s* heap, void* a, void* b)
{
  int temp = *(int*)a;
  *(int*)a = *(int*)b;
  *(int*)b = temp;
  return ;
}

void heap_cpu()
{
    int a = 0;
    void* p = 0;
    int i = 0;
    int to = 0;
	unsigned int old =  ox_getnowtime();
	struct heap_s* heap = ox_heap_new(100000, sizeof(int), compare_int, swap_int, NULL);
    
	old =  ox_getnowtime();
    printf("开始添加堆元素:%d\n", old);   
    for(; i < 1000000; ++i)
    {
        a = rand() % 10000;
		ox_heap_insert(heap, &a);
    }

    old =  ox_getnowtime();
    printf("添加完毕，开始pop:%d\n", old);  
	while( p =ox_heap_pop(heap))
    {
        to++;
            //int temp = *(int*)p;
            //printf("%d\n", temp);
        ;
    }
    
    old =  ox_getnowtime();
    printf("pop完毕:%d, to:%d\n", old, to);  
}

int main()
{
    srand((int)time(0));
    test_timer_cpu();
    heap_cpu();
    typepool_cpu();
    return 0;
}
