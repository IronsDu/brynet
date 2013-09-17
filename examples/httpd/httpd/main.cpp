#include "thread.h"
#include "http_server.h"

// ¼àÌý¶Ë¿Ú4000
const static int HTTP_PORT = 4000;

int main()
{
    http_server_init(HTTP_PORT);
    http_server_start();

    while(1)
    {
        thread_sleep(1);
    }

    http_server_stop();

    return 0;
}
