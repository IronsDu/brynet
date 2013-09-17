#ifndef _SERVER_H_INCLUDED_
#define _SERVER_H_INCLUDED_

#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s;

typedef void (*logic_on_enter_pt)(struct server_s* self, int index);
typedef void (*logic_on_close_pt)(struct server_s* self, int index);
typedef int (*logic_on_recved_pt)(struct server_s* self, int index, const char* buffer, int len);

void server_start(struct server_s* self,
                  logic_on_enter_pt enter_pt,
                  logic_on_close_pt close_pt,
                  logic_on_recved_pt    recved_pt);

void server_stop(struct server_s* self);
void server_close(struct server_s* self, int index);
bool server_send(struct server_s* self, int index, const char* data, int len);

#ifdef  __cplusplus
}
#endif

#endif
