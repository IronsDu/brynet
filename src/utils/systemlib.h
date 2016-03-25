#ifndef _SYSTEMLIB_H
#define _SYSTEMLIB_H

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

int64_t ox_getnowtime(void);
int ox_getcpunum(void);

#ifdef  __cplusplus
}
#endif

#endif
