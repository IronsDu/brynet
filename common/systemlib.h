#ifndef _SYSTEMLIB_H
#define _SYSTEMLIB_H

#include <stdint.h>

#include "dllconf.h"

#ifdef  __cplusplus
extern "C" {
#endif

DLL_CONF int64_t ox_getnowtime(void);
DLL_CONF int ox_getcpunum(void);

#ifdef  __cplusplus
}
#endif

#endif
