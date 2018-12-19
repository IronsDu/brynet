#pragma once

#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct stack_s;

struct stack_s* ox_stack_new(int num, int element_size);
void ox_stack_init(struct stack_s* self);
void ox_stack_delete(struct stack_s* self);
bool ox_stack_push(struct stack_s* self, const void* data);
char* ox_stack_front(struct stack_s* self);
char* ox_stack_popfront(struct stack_s* self);
char* ox_stack_popback(struct stack_s* self);
bool ox_stack_isfull(struct stack_s* self);
bool ox_stack_increase(struct stack_s* self, int increase_num);
int ox_stack_size(struct stack_s* self);
int ox_stack_num(struct stack_s* self);

#ifdef  __cplusplus
}
#endif
