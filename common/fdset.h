#ifndef _FDSET_INCLUDED_H
#define _FDSET_INCLUDED_H

#include "dllconf.h"
#include "stdbool.h"
#include "socketlibtypes.h"

enum CheckType
{
    ReadCheck = 0x1,
    WriteCheck = 0x2,
    ErrorCheck = 0x4,
};

#ifdef  __cplusplus
extern "C" {
#endif

struct fdset_s;

DLL_CONF struct fdset_s* ox_fdset_new(void);
DLL_CONF fd_set* ox_fdset_getresult(struct fdset_s* self, enum CheckType type);
DLL_CONF void ox_fdset_delete(struct fdset_s* self);
DLL_CONF void ox_fdset_add(struct fdset_s* self, sock fd, int type);
DLL_CONF void ox_fdset_del(struct fdset_s* self, sock fd, int type);
DLL_CONF int ox_fdset_poll(struct fdset_s* self, long overtime);
DLL_CONF bool ox_fdset_check(struct fdset_s* self, sock fd, enum CheckType type);

#ifdef  __cplusplus
}
#endif

#endif
