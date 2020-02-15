#pragma once

#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdbool>
#include <cstring>

#include <brynet/net/SocketLibTypes.hpp>

#ifdef BRYNET_PLATFORM_LINUX
#include <endian.h>
#elif defined BRYNET_PLATFORM_DARWIN
#include <sys/_endian.h>
#endif

namespace brynet { namespace base { namespace endian {

    inline uint64_t  hl64ton(uint64_t   hostValue)
    {
        uint64_t   ret = 0;
        uint32_t   high, low;

        low = hostValue & 0xFFFFFFFF;
        high = (hostValue >> 32) & 0xFFFFFFFF;
        low = htonl(low);
        high = htonl(high);
        ret = low;
        ret <<= 32;
        ret |= high;

        return   ret;
    }

    inline uint64_t  ntohl64(uint64_t   netValue)
    {
        uint64_t   ret = 0;
        uint32_t   high, low;

        low = netValue & 0xFFFFFFFF;
        high = (netValue >> 32) & 0xFFFFFFFF;
        low = ntohl(low);
        high = ntohl(high);
        ret = low;
        ret <<= 32;
        ret |= high;

        return   ret;
    }

#ifdef BRYNET_PLATFORM_WINDOWS
    inline uint64_t hostToNetwork64(uint64_t host64, bool convert = true)
    {
        return convert ? hl64ton(host64) : host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32, bool convert = true)
    {
        return convert ? htonl(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16, bool convert = true)
    {
        return convert ? htons(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64, bool convert = true)
    {
        return convert ? ntohl64(net64) : net64;
    }

    inline uint32_t networkToHost32(uint32_t net32, bool convert = true)
    {
        return convert ? ntohl(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16, bool convert = true)
    {
        return convert ? ntohs(net16) : net16;
    }
#elif defined BRYNET_PLATFORM_LINUX
    inline uint64_t hostToNetwork64(uint64_t host64, bool convert = true)
    {
        return convert ? htobe64(host64) : host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32, bool convert = true)
    {
        return convert ? htobe32(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16, bool convert = true)
    {
        return convert ? htobe16(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64, bool convert = true)
    {
        return convert ? be64toh(net64) : net64;
    }

    inline uint32_t networkToHost32(uint32_t net32, bool convert = true)
    {
        return convert ? be32toh(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16, bool convert = true)
    {
        return convert ? be16toh(net16) : net16;
    }
#elif defined BRYNET_PLATFORM_DARWIN
    inline uint64_t hostToNetwork64(uint64_t host64, bool convert = true)
    {
        return convert ? hl64ton(host64) : host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32, bool convert = true)
    {
        return convert ? htonl(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16, bool convert = true)
    {
        return convert ? htons(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64, bool convert = true)
    {
        return convert ? ntohl64(net64) : net64;
    }

    inline uint32_t networkToHost32(uint32_t net32, bool convert = true)
    {
        return convert ? ntohl(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16, bool convert = true)
    {
        return convert ? ntohs(net16) : net16;
    }
#endif

} } }