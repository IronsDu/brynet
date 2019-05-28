#pragma once

#include <stdbool.h>
#include <brynet/net/SocketLibTypes.h>
#include <brynet/utils/stack.h>

enum CheckType
{
    ReadCheck = 0x1,
    WriteCheck = 0x2,
    ErrorCheck = 0x4,
};

#ifdef  __cplusplus
extern "C" {
#endif

struct poller_s;

struct poller_s* ox_poller_new(void);
void ox_poller_delete(struct poller_s* self);
void ox_poller_add(struct poller_s* self, sock fd, int type);
void ox_poller_del(struct poller_s* self, sock fd, int type);
void ox_poller_remove(struct poller_s* self, sock fd);
void ox_poller_visitor(struct poller_s* self, enum CheckType type, struct stack_s* result);
int ox_poller_poll(struct poller_s* self, long overtime);
bool ox_poller_check(struct poller_s* self, sock fd, enum CheckType type);

#ifdef  __cplusplus
}
#endif
