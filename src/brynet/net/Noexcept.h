#ifndef _BRYNET_NET_NOEXCEPT_H
#define _BRYNET_NET_NOEXCEPT_H

#include <brynet/utils/CPP_VERSION.h>

#ifdef HAVE_LANG_CXX17
#define BRYNET_NOEXCEPT noexcept
#else
#define BRYNET_NOEXCEPT
#endif

#endif
