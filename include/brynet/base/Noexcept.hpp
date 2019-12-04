#pragma once

#include <brynet/base/CPP_VERSION.hpp>

#ifdef BRYNET_HAVE_LANG_CXX17
#define BRYNET_NOEXCEPT noexcept
#else
#define BRYNET_NOEXCEPT
#endif
