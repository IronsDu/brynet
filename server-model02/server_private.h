#ifndef _SERVER_PRIVATE_H_INCLUIDED
#define _SERVER_PRIVATE_H_INCLUIDED

#include <stdint.h>
#include "server.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s
{
    void    (*start_pt)(struct server_s* self,  logic_on_enter_pt enter_pt, logic_on_close_pt close_pt, logic_on_recved_pt  recved_pt, logic_on_cansend_pt cansend_pt, logic_on_sendfinish_pt  sendfinish_pt);
    void    (*poll_pt)(struct server_s* self, int64_t timeout);
    void    (*stop_pt)(struct server_s* self);
    void    (*closesession_pt)(struct server_s* self, int index);
    bool    (*register_pt)(struct server_s* self, int fd);
    int    (*send_pt)(struct server_s* self, int index, const char* data, int len);
    int     (*sendv_pt)(struct server_s* self, int index, const char* datas[], const int* lens, int num);
    int     (*copy_pt)(struct server_s* self, int index, const char* data, int len);

    logic_on_enter_pt   logic_on_enter;
    logic_on_close_pt   logic_on_close;
    logic_on_recved_pt  logic_on_recved;
    logic_on_cansend_pt logic_on_cansend;
    logic_on_sendfinish_pt  logic_on_sendfinish;
    
    void*   ext;
};

#ifdef  __cplusplus
}
#endif

#endif
