#ifndef _STACK_H_INCLUDED_
#define _STACK_H_INCLUDED_

#include "stdbool.h"
#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct stack_s;

DLL_CONF    struct stack_s* ox_stack_new(int num, int element_size);
DLL_CONF    void ox_stack_init(struct stack_s* self);
DLL_CONF    void ox_stack_delete(struct stack_s* self);
DLL_CONF    bool ox_stack_push(struct stack_s* self, const void* data);
DLL_CONF    char* ox_stack_front(struct stack_s* self);
DLL_CONF    char* ox_stack_popfront(struct stack_s* self);
DLL_CONF    char* ox_stack_popback(struct stack_s* self);
DLL_CONF    bool ox_stack_isfull(struct stack_s* self);
DLL_CONF    bool ox_stack_increase(struct stack_s* self, int increase_num);
DLL_CONF    int ox_stack_size(struct stack_s* self);
DLL_CONF    int ox_stack_num(struct stack_s* self);

#ifdef  __cplusplus
}
#endif

#endif
