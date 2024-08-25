﻿#pragma once

#include <brynet/net/SocketLibTypes.hpp>

namespace brynet { namespace net { namespace port {

#ifdef BRYNET_PLATFORM_WINDOWS
class Win
{
public:
    enum class OverlappedType
    {
        OverlappedNone = 0,
        OverlappedRecv,
        OverlappedSend,
    };

    struct OverlappedExt
    {
        OVERLAPPED base;
        const OverlappedType OP;

        OverlappedExt(OverlappedType op)
            : OP(op)
        {
            memset(&base, 0, sizeof(base));
        }
    };
};
#endif

}}}// namespace brynet::net::port
