#include "server.h"
#include "server_private.h"

void server_start(struct server_s* self,
                  logic_on_enter_handle enter_callback,
                  logic_on_disconnection_handle disconnection_callback,
                  logic_on_recved_handle    recved_callback,
                  logic_on_sendfinish_handle  sendfinish_callback,
                  logic_on_close_completed  closecompleted_callback)
{
    (self->start_callback)(self, enter_callback, disconnection_callback, recved_callback, sendfinish_callback, closecompleted_callback);
}

void server_pool(struct server_s* self, int timeout)
{
    (self->poll_callback)(self, timeout);
}

void server_stop(struct server_s* self)
{
    (self->stop_callback)(self);
}

void server_close(struct server_s* self, void* handle)
{
    (self->closesession_callback)(self, handle);
}

bool server_register(struct server_s* self, void* ud, int fd)
{
    return (self->register_callback)(self, ud, fd);
}

int server_sendv(struct server_s* self, void* handle, const char* datas[], const int* lens, int num)
{
    return (self->sendv_callback)(self, handle, datas, lens, num);
}

void* server_getext(struct server_s* self)
{
    return self->ext;
}

