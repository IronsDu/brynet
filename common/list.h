#ifndef _LIST_H_INCLUDED_
#define _LIST_H_INCLUDED_

#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct list_node_s;
struct list_s;

struct list_node_s
{
    void*   data;
    struct list_node_s* prior;
    struct list_node_s* next;
};

DLL_CONF struct list_s* ox_list_new(int node_num, int element_size);
DLL_CONF void ox_list_delete(struct list_s* self);
DLL_CONF bool ox_list_push_back(struct list_s* self, const void* data);
DLL_CONF struct list_node_s* ox_list_erase(struct list_s* self, struct list_node_s* node);
DLL_CONF struct list_node_s* ox_list_begin(struct list_s* self);
DLL_CONF const struct list_node_s* ox_list_end(struct list_s* self);

#ifdef  __cplusplus
}
#endif

#endif
