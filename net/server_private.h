#ifndef _SERVER_PRIVATE_H_INCLUIDED
#define _SERVER_PRIVATE_H_INCLUIDED

#include <stdint.h>
#include "server.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s
{
    void    (*start_callback)(struct server_s* self,  logic_on_enter_handle enter_pt, logic_on_close_handle close_pt, logic_on_recved_handle  recved_pt, logic_on_cansend_handle cansend_pt, logic_on_sendfinish_handle  sendfinish_pt);
    void    (*poll_callback)(struct server_s* self, int64_t timeout);
    void    (*stop_callback)(struct server_s* self);
    void    (*closesession_callback)(struct server_s* self, void* handle);
    bool    (*register_callback)(struct server_s* self, void* ud, int fd);
    int     (*sendv_callback)(struct server_s* self, void* handle, const char* datas[], const int* lens, int num);

    logic_on_enter_handle   logic_on_enter;
    logic_on_close_handle   logic_on_close;
    logic_on_recved_handle  logic_on_recved;
    logic_on_cansend_handle logic_on_cansend;
    logic_on_sendfinish_handle  logic_on_sendfinish;
    
    void*   ext;
};

#ifdef  __cplusplus
}
#endif

#endif
