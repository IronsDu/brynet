#ifndef _SERVER_PRIVATE_H_INCLUIDED
#define _SERVER_PRIVATE_H_INCLUIDED

#include "server.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct server_s
{
    void    (*start_pt)(struct server_s* self,  logic_on_enter_pt enter_pt, logic_on_close_pt close_pt, logic_on_recved_pt  recved_pt);
    void    (*stop_pt)(struct server_s* self);
    void    (*closesession_pt)(struct server_s* self, int index);
    int    (*send_pt)(struct server_s* self, int index, const char* data, int len);

    logic_on_enter_pt   logic_on_enter;
    logic_on_close_pt   logic_on_close;
    logic_on_recved_pt  logic_on_recved;
};

#ifdef  __cplusplus
}
#endif

#endif
