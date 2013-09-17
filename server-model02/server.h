#ifndef _SERVER_H_INCLUDED_
#define _SERVER_H_INCLUDED_

#include <stdbool.h>
#include <stdint.h>

#define MAX_SENDBUF_NUM (1024)

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s;

typedef void (*logic_on_enter_pt)(struct server_s* self, int index);
typedef void (*logic_on_close_pt)(struct server_s* self, int index);
typedef int (*logic_on_recved_pt)(struct server_s* self, int index, const char* buffer, int len);
typedef void (*logic_on_cansend_pt)(struct server_s* self, int index);
typedef void (*logic_on_sendfinish_pt)(struct server_s* self, int index, int len);

void server_start(struct server_s* self,
                  logic_on_enter_pt enter_pt,
                  logic_on_close_pt close_pt,
                  logic_on_recved_pt    recved_pt,
                  logic_on_cansend_pt canrecv_pt,
                  logic_on_sendfinish_pt  sendfinish_pt);

void server_pool(struct server_s* self, int timeout);
void server_stop(struct server_s* self);
void server_close(struct server_s* self, int index);
bool server_register(struct server_s* self, int fd);
int server_send(struct server_s* self, int index, const char* data, int len);
int server_sendv(struct server_s* self, int index, const char* datas[], const int* lens, int num);
int server_copy(struct server_s* self, int index, const char* data, int len);

void* server_getext(struct server_s* self);

#ifdef  __cplusplus
}
#endif

#endif
