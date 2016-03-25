#include "platform.h"

#ifdef PLATFORM_WINDOWS
#include <direct.h>
#include <io.h>

#define ACCESS _access
#define MKDIR(dir_name) _mkdir(dir_name)

#else
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#define ACCESS access
#define MKDIR(dir_name) mkdir(dir_name, 0755)
#endif

#include "ox_file.h"

bool ox_dir_create(const char* dir)
{
    bool ret = false;
    if(ox_file_access(dir))
    {
        ret = true;
    }
    else
    {
        ret = (MKDIR(dir)) == 0;
    }

    return ret;
}

bool ox_file_access(const char* filename)
{
    return ACCESS(filename, 0) == 0;
}