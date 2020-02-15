#pragma once

#include <cstdbool>
#include <cstdio>
#include <signal.h>

#include <brynet/base/Platform.hpp>

#ifdef BRYNET_PLATFORM_WINDOWS
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace brynet { namespace base {

    static bool app_kbhit()
    {
#ifdef BRYNET_PLATFORM_WINDOWS
        return _kbhit();
#else
        struct termios oldt;
        tcgetattr(STDIN_FILENO, &oldt);
        auto newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        const auto oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        const auto ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);

        if (ch != EOF)
        {
            ungetc(ch, stdin);
            return true;
        }

        return false;
#endif
    }

} }