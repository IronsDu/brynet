#ifndef _PACKET_H
#define _PACKET_H

#include <stdint.h>
#include <assert.h>
#include <string>
#include <stdbool.h>

#include "SocketLibTypes.h"

namespace socketendian
{
    const static bool IS_ENDIAN = false;

#ifdef PLATFORM_WINDOWS
    inline uint64_t hostToNetwork64(uint64_t host64)
    {
        return host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32)
    {
        return IS_ENDIAN ? htonl(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16)
    {
        return IS_ENDIAN ? htons(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64)
    {
        return net64;
    }

    inline uint32_t networkToHost32(uint32_t net32)
    {
        return IS_ENDIAN ? ntohl(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16)
    {
        return IS_ENDIAN ? ntohs(net16) : net16;
    }
#else
    inline uint64_t hostToNetwork64(uint64_t host64)
    {
        return IS_ENDIAN ? htonll(host64) : host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32)
    {
        return IS_ENDIAN ? htonl(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16)
    {
        return IS_ENDIAN ? htons(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64)
    {
        return IS_ENDIAN ? ntohll(net64) : net64;
    }

    inline uint32_t networkToHost32(uint32_t net32)
    {
        return IS_ENDIAN ? ntohl(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16)
    {
        return IS_ENDIAN ? ntohs(net16) : net16;
    }
#endif
}

class Packet
{
public:
    Packet(char* buffer, int len)
    {
        mMaxLen = len;
        mPos = 0;
        mBuffer = buffer;
        mIsFinish = false;
    }
    
    virtual ~Packet()
    {
        if (!mIsFinish)
        {
            assert(mIsFinish);
        }
    }
    
    void    init()
    {
        mPos = 0;
    }
    
    void    setOP(int16_t op)
    {
        assert(mPos == 0);
        writeINT16(sizeof(int16_t)+sizeof(int16_t));
        writeINT16(op);
    }
    
    void    writeINT8(int8_t value)
    {
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT8(uint8_t value)
    {
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeINT16(int16_t value)
    {
        value = socketendian::hostToNetwork16(value);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT16(uint16_t value)
    {
        value = socketendian::hostToNetwork16(value);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeINT32(int32_t value)
    {
        value = socketendian::hostToNetwork32(value);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT32(uint32_t value)
    {
        value = socketendian::hostToNetwork32(value);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeINT64(int64_t value)
    {
        value = socketendian::hostToNetwork64(value);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT64(uint64_t value)
    {
        value = socketendian::hostToNetwork64(value);
        writeBuffer((char*)&value, sizeof(value));
    }
    /*写入二进制字符串，并记录其长度，反序列方使用ReadPacket::readBinary()读取*/
    void    writeBinary(const std::string& binary)
    {
        writeINT16(binary.size());
        writeBuffer(binary.c_str(), binary.size());
    }
    void    writeBinary(const char* binary, size_t binaryLen)
    {
        writeINT16(binaryLen);
        writeBuffer(binary, binaryLen);
    }

    /*writev接口写入可变参数*/
    template<typename Arg1, typename... Args>
    void            writev(const Arg1& arg1, const Args&... args)
    {
        this->operator<<(arg1);
        writev(args...);
    }

    void            writev()
    {
    }

    Packet & operator << (const int8_t &v)
    {
        writeINT8(v);
        return *this;
    }
    Packet & operator << (const int16_t &v)
    {
        writeINT16(v);
        return *this;
    }
    Packet & operator << (const int32_t &v)
    {
        writeINT32(v);
        return *this;
    }
    Packet & operator << (const int64_t &v)
    {
        writeINT64(v);
        return *this;
    }
    Packet & operator << (const char* const &v)
    {
        writeBinary(v);
        return *this;
    }
    Packet & operator << (const std::string &v)
    {
        writeBinary(v);
        return *this;
    }
    /*写入自定义类型对象的内存区，消息包里不记录其长度，反序列方则使用ReadPacket::read<T>(v)进行读取*/
    template<typename T>
    Packet & operator << (const T& v)
    {
        static_assert(std::is_same < T, std::remove_pointer<T>::type>::value, "T must a nomal type");
        static_assert(std::is_pod <T>::value, "T must a pod type");
        writeBuffer((const char*)&v, sizeof(v));
        return *this;
    }

    void        end()
    {
        uint16_t len = socketendian::hostToNetwork16(mPos);
        if (sizeof(len) <= mMaxLen)
        {
            memcpy(mBuffer, &len, sizeof(len));
            mIsFinish = true;
        }
    }

    int         getLen()
    {
        return mPos;
    }

    const char*    getData()
    {
        return mBuffer;
    }

private:
    /*直接写入二进制流，消息包里不记录其长度*/
    void    writeBuffer(const char* buffer, int len)
    {
        assert((mPos + len) <= mMaxLen);
        if ((mPos + len) <= mMaxLen)
        {
            memcpy(mBuffer + mPos, buffer, len);
            mPos += len;
        }
    }

private:
    bool        mIsFinish;
    int         mPos;
    int         mMaxLen;
    char*       mBuffer;
};

class ReadPacket
{
public:
    ReadPacket(const char* buffer, int len)
    {
        mMaxLen = len;
        mPos = 0;
        mBuffer = buffer;
    }

    ~ReadPacket()
    {
        if (mPos != mMaxLen)
        {
            assert(mPos == mMaxLen);
        }
    }
    
    int8_t      readINT8()
    {
        int8_t  value = 0;
        read(value);
        return value;
    }
    uint8_t      readUINT8()
    {
        uint8_t  value = 0;
        read(value);
        return value;
    }
    int16_t      readINT16()
    {
        int16_t  value = 0;
        read(value);
        return socketendian::networkToHost16(value);
    }
    uint16_t      readUINT16()
    {
        uint16_t  value = 0;
        read(value);
        return socketendian::networkToHost16(value);
    }
    int32_t      readINT32()
    {
        int32_t  value = 0;
        read(value);
        return socketendian::networkToHost32(value);
    }
    uint32_t      readUINT32()
    {
        uint32_t  value = 0;
        read(value);
        return socketendian::networkToHost32(value);
    }
    int64_t      readINT64()
    {
        int64_t  value = 0;
        read(value);
        return socketendian::networkToHost64(value);
    }
    uint64_t      readUINT64()
    {
        uint64_t  value = 0;
        read(value);
        return socketendian::networkToHost64(value);
    }
    /*读取二进制字符串*/
    std::string     readBinary()
    {
        std::string ret;
        int len = readINT16();
        if ((mPos + len) <= mMaxLen)
        {
            ret = std::string((const char*)(mBuffer + mPos), len);
            mPos += len;
        }
        
        return ret;
    }
    
    /*读取某自定义类型*/
    template<typename T>
    void           read(T& value)
    {
        static_assert(std::is_same < T, std::remove_pointer<T>::type>::value, "T must a nomal type");
        static_assert(std::is_pod <T>::value, "T must a pod type");
        
        if ((mPos + sizeof(value) <= mMaxLen))
        {
            value = *(T*)(mBuffer + mPos);
            mPos += sizeof(value);
        }
        else
        {
            assert(mPos + sizeof(value) <= mMaxLen);
        }
    }

    void            skipAll()
    {
        mPos = mMaxLen;
    }
private:
    size_t          mPos;
    size_t          mMaxLen;
    const char*     mBuffer;
};

template<int SIZE>
class FixedPacket : public Packet
{
public:
    FixedPacket() : Packet(mData, SIZE)
    {}
    
private:
    char        mData[SIZE];
};

typedef FixedPacket<128>        TinyPacket;
typedef FixedPacket<256>        ShortPacket;
typedef FixedPacket<512>        MiddlePacket;
typedef FixedPacket<1024>       LongPacket;
typedef FixedPacket<16*1024>    BigPacket;

#endif