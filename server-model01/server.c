#include "server.h"
#include "server_private.h"

void server_start(struct server_s* self,
                  logic_on_enter_pt enter_pt,
                  logic_on_close_pt close_pt,
                  logic_on_recved_pt    recved_pt)
{
    (self->start_pt)(self, enter_pt, close_pt, recved_pt);
}

void server_stop(struct server_s* self)
{
    (self->stop_pt)(self);
}

void server_close(struct server_s* self, int index)
{
    (self->closesession_pt)(self, index);
}

bool server_send(struct server_s* self, int index, const char* data, int len)
{
    return (self->send_pt)(self, index, data, len);
}

