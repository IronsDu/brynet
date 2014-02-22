#ifndef _SERVER_H_INCLUDED_
#define _SERVER_H_INCLUDED_

#include <stdbool.h>
#include <stdint.h>

#define MAX_SENDBUF_NUM (1024)

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s;

typedef void (*logic_on_enter_handle)(struct server_s* self, void* ud, void* handle);
typedef void (*logic_on_disconnection_handle)(struct server_s* self, void* ud);
typedef int (*logic_on_recved_handle)(struct server_s* self, void* ud, const char* buffer, int len);
typedef void (*logic_on_cansend_handle)(struct server_s* self, void* ud);
typedef void (*logic_on_sendfinish_handle)(struct server_s* self, void* ud, int len);
typedef void (*logic_on_close_completed)(struct server_s* self, void* ud);

void server_start(struct server_s* self,
                  logic_on_enter_handle enter_callback,
                  logic_on_disconnection_handle close_callback,
                  logic_on_recved_handle    recved_callback,
                  logic_on_cansend_handle canrecv_callback,
                  logic_on_sendfinish_handle  sendfinish_callback,
                  logic_on_close_completed  closecompleted_callback);

void server_pool(struct server_s* self, int timeout);
void server_stop(struct server_s* self);
void server_close(struct server_s* self, void* handle);
bool server_register(struct server_s* self, void* ud, int fd);
int server_sendv(struct server_s* self, void* handle, const char* datas[], const int* lens, int num);

void* server_getext(struct server_s* self);

#ifdef  __cplusplus
}
#endif

#endif
