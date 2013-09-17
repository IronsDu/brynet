#ifndef _ARRAY_H_INCLUDED_
#define _ARRAY_H_INCLUDED_

#include "dllconf.h"
#include "stdbool.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct array_s;

DLL_CONF struct array_s* ox_array_new(int num, int element_size);
DLL_CONF void ox_array_delete(struct array_s* self);
DLL_CONF char* ox_array_at(struct array_s* self, int index);
DLL_CONF bool ox_array_set(struct array_s* self, int index, const void* data);
DLL_CONF bool ox_array_increase(struct array_s* self, int increase_num);
DLL_CONF int ox_array_num(struct array_s* self);

#ifdef  __cplusplus
}
#endif

#endif
