#pragma once

#include <brynet/utils/CPP_VERSION.h>

#ifdef HAVE_LANG_CXX17
#define BRYNET_NOEXCEPT noexcept
#else
#define BRYNET_NOEXCEPT
#endif
