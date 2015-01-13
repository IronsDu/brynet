#include <stdio.h>

#include "heap.h"

bool compare_int(struct heap_s* self, const void* a, const void* b)
{
  int aint = *(int*)a;
  int bint = *(int*)b;

  return aint > bint;
}


void swap_int(struct heap_s* self, void* a, void* b)
{
  int temp = *(int*)a;
  *(int*)a = *(int*)b;
  *(int*)b = temp;
  return ;
}

int main()
{
  int a = 3;
  int b = 1;
  int c = 10;
  struct heap_s* heap = ox_heap_new(10, sizeof(int), compare_int, swap_int, NULL);
 
  ox_heap_insert(heap, &a);
  ox_heap_insert(heap, &b);
  ox_heap_insert(heap, &c);
  ox_heap_insert(heap, &c);
  ox_heap_insert(heap, &c);
  ox_heap_insert(heap, &a);

  while (true)
  {
    void* p = ox_heap_pop(heap);
    if(p != 0)
    {
      int temp = *(int*)p;
      printf("%d\n", temp);
    }
  }

  return 0;
}