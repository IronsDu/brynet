#include <stdio.h>
#include <signal.h>

#if defined(_MSC_VER)
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include "app_status.h"

struct app_status
{
    bool    active;
};

static struct app_status s_app;

static void
cc_handle(int sigid)
{
    if(sigid == SIGINT)
    {
        s_app.active = false;
    }
}

void
app_init()
{
    s_app.active = true;
    signal(SIGINT, cc_handle);
}

bool
app_getstatus()
{
    return s_app.active;
}

int app_kbhit(void)
{
#if defined(_MSC_VER)
    return _kbhit();
#else
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
#endif
}
