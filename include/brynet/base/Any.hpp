#pragma once

#include <brynet/base/CPP_VERSION.hpp>

#ifdef BRYNET_HAVE_LANG_CXX17
#include <any>
#else
#include <cstdint>
#endif

namespace brynet { namespace base {

#ifdef BRYNET_HAVE_LANG_CXX17
    using BrynetAny = std::any;

    template<typename T>
    auto cast(const BrynetAny& ud)
    {
        return std::any_cast<T>(&ud);
    }
#else
    using BrynetAny = int64_t;
    template<typename T>
    const T* cast(const BrynetAny& ud)
    {
        return static_cast<const T*>(&ud);
    }
#endif

} }
