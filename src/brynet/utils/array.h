#pragma once

#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct array_s;

struct array_s* ox_array_new(int num, int element_size);
void ox_array_delete(struct array_s* self);
char* ox_array_at(struct array_s* self, int index);
bool ox_array_set(struct array_s* self, int index, const void* data);
bool ox_array_increase(struct array_s* self, int increase_num);
int ox_array_num(const struct array_s* self);

#ifdef  __cplusplus
}
#endif
