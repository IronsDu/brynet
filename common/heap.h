#ifndef _HEAP_H_INCLUDED_
#define _HEAP_H_INCLUDED_

#include <stdbool.h>

#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct heap_s;

typedef bool (*pfn_compare)(struct heap_s* self, const void* a, const void* b);
typedef void (*pfn_swap)(struct heap_s* self, void* a, void* b);

DLL_CONF    struct heap_s* ox_heap_new(int element_num, int element_size, pfn_compare compare, pfn_swap swap, void* ext);
DLL_CONF    void ox_heap_delete(struct heap_s* self);

DLL_CONF    bool ox_heap_insert(struct heap_s* self, const void* datap);

DLL_CONF    void* ox_heap_top(struct heap_s* self);
DLL_CONF    void* ox_heap_pop(struct heap_s* self);

DLL_CONF    void ox_heap_clear(struct heap_s* self);
DLL_CONF    bool ox_heap_increase(struct heap_s* self, int incnum);

DLL_CONF    void* ox_heap_getext(struct heap_s* self);

#ifdef  __cplusplus
}
#endif

#endif