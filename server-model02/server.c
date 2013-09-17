#include "server.h"
#include "server_private.h"

void server_start(struct server_s* self,
                  logic_on_enter_pt enter_pt,
                  logic_on_close_pt close_pt,
                  logic_on_recved_pt    recved_pt,
                  logic_on_cansend_pt cansend_pt,
                  logic_on_sendfinish_pt  sendfinish_pt)
{
    (self->start_pt)(self, enter_pt, close_pt, recved_pt, cansend_pt, sendfinish_pt);
}

void server_pool(struct server_s* self, int timeout)
{
    (self->poll_pt)(self, timeout);
}

void server_stop(struct server_s* self)
{
    (self->stop_pt)(self);
}

void server_close(struct server_s* self, int index)
{
    (self->closesession_pt)(self, index);
}

bool server_register(struct server_s* self, int fd)
{
    return (self->register_pt)(self, fd);
}

int server_send(struct server_s* self, int index, const char* data, int len)
{
    return (self->send_pt)(self, index, data, len);
}

int server_sendv(struct server_s* self, int index, const char* datas[], const int* lens, int num)
{
    return (self->sendv_pt)(self, index, datas, lens, num);
}

int server_copy(struct server_s* self, int index, const char* data, int len)
{
    return (self->copy_pt)(self, index, data, len);
}

void* server_getext(struct server_s* self)
{
    return self->ext;
}

