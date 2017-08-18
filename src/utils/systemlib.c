#include "systemlib.h"
#include "Platform.h"
/*  thanks eathena  */

#if defined PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h> // struct timeval, gettimeofday()
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

int 
ox_getcpunum(void)
{
    int processorNum = 1;
#if defined PLATFORM_WINDOWS
    SYSTEM_INFO    si;
    GetSystemInfo( &si );
    processorNum = si.dwNumberOfProcessors;
#else

    FILE * pFile = popen("cat /proc/cpuinfo | grep processor | wc -l", "r");
    if(pFile != NULL)
    {
        char buff[1024];
        fgets(buff, sizeof(buff), pFile);
        pclose(pFile);
        processorNum = atoi(buff);
    }
#endif

    return processorNum;
}

