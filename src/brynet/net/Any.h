#ifndef _BRYNET_NET_ANY_H
#define _BRYNET_NET_ANY_H

#if (__cplusplus >= 201703L || \
     (defined(_MSC_VER) && _MSC_VER >= 1910))
// Define this to 1 if the code is compiled in C++11 mode; leave it
// undefined otherwise.  Do NOT define it to 0 -- that causes
// '#ifdef LANG_CXX11' to behave differently from '#if LANG_CXX11'.
#define LANG_CXX17 1
#endif

#ifdef LANG_CXX17
#include <any>
#else
#include <cstdint>
#endif // LANG_CXX17

namespace brynet
{
    namespace net
    {
#ifdef LANG_CXX17
        typedef std::any BrynetAny;

        template<typename T>
        auto cast(const BrynetAny& ud)
        {
            return std::any_cast<T>(&ud);
        }
#else
        typedef int64_t BrynetAny;
        template<typename T>
        auto cast(const BrynetAny& ud)
        {
            return static_cast<const T*>(&ud);
        }
#endif
    }
}

#endif
