#ifndef _APP_STATUS_H
#define _APP_STATUS_H

/*  ½ø³Ì×´Ì¬¿ØÖÆ  */

#include <stdbool.h>

#ifdef  __cplusplus
extern "C" {
#endif

void    app_init();

bool    app_getstatus();

int     app_kbhit(void);

#ifdef  __cplusplus
}
#endif

#endif