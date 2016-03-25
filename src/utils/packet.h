#ifndef _PACKET_H
#define _PACKET_H

#include <stdint.h>
#include <assert.h>
#include <string>
#include <stdbool.h>

#include "socketlibtypes.h"

#ifndef PLATFORM_WINDOWS
#include <endian.h>
#endif

typedef  uint32_t PACKET_LEN_TYPE;
typedef  uint16_t PACKET_OP_TYPE;

const static PACKET_LEN_TYPE PACKET_HEAD_LEN = sizeof(PACKET_LEN_TYPE) + sizeof(PACKET_OP_TYPE);

namespace socketendian
{
    const static bool ENABLE_CONVERT_ENDIAN = false;

    inline uint64_t  hl64ton(uint64_t   host)
    {
        uint64_t   ret = 0;
        uint32_t   high, low;

        low = host & 0xFFFFFFFF;
        high = (host >> 32) & 0xFFFFFFFF;
        low = htonl(low);
        high = htonl(high);
        ret = low;
        ret <<= 32;
        ret |= high;

        return   ret;
    }

    inline uint64_t  ntohl64(uint64_t   host)
    {
        uint64_t   ret = 0;
        uint32_t   high, low;

        low = host & 0xFFFFFFFF;
        high = (host >> 32) & 0xFFFFFFFF;
        low = ntohl(low);
        high = ntohl(high);
        ret = low;
        ret <<= 32;
        ret |= high;

        return   ret;
    }

#ifdef PLATFORM_WINDOWS
    inline uint64_t hostToNetwork64(uint64_t host64)
    {
        return ENABLE_CONVERT_ENDIAN ? hl64ton(host64) : host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32)
    {
        return ENABLE_CONVERT_ENDIAN ? htonl(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16)
    {
        return ENABLE_CONVERT_ENDIAN ? htons(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64)
    {
        return ENABLE_CONVERT_ENDIAN ? ntohl64(net64) : net64;
    }

    inline uint32_t networkToHost32(uint32_t net32)
    {
        return ENABLE_CONVERT_ENDIAN ? ntohl(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16)
    {
        return ENABLE_CONVERT_ENDIAN ? ntohs(net16) : net16;
    }
#else
    inline uint64_t hostToNetwork64(uint64_t host64)
    {
        return ENABLE_CONVERT_ENDIAN ? htobe64(host64) : host64;
    }
    inline uint32_t hostToNetwork32(uint32_t host32)
    {
        return ENABLE_CONVERT_ENDIAN ? htobe32(host32) : host32;
    }

    inline uint16_t hostToNetwork16(uint16_t host16)
    {
        return ENABLE_CONVERT_ENDIAN ? htobe16(host16) : host16;
    }

    inline uint64_t networkToHost64(uint64_t net64)
    {
        return ENABLE_CONVERT_ENDIAN ? be64toh(net64) : net64;
    }

    inline uint32_t networkToHost32(uint32_t net32)
    {
        return ENABLE_CONVERT_ENDIAN ? be32toh(net32) : net32;
    }

    inline uint16_t networkToHost16(uint16_t net16)
    {
        return ENABLE_CONVERT_ENDIAN ? be16toh(net16) : net16;
    }
#endif
}

class Packet
{
public:
    Packet(char* buffer, size_t len, bool isAutoMalloc = false) : mIsAutoMalloc(isAutoMalloc)
    {
        mMaxLen = len;
        mPos = 0;
        mBuffer = buffer;
        mIsFinish = false;
        mMallocBuffer = nullptr;
    }
    
    virtual ~Packet()
    {
        if (!mIsFinish)
        {
            assert(mIsFinish);
        }

        if (mMallocBuffer != nullptr)
        {
            free(mMallocBuffer);
            mMallocBuffer = nullptr;
        }
    }
    
    void    init()
    {
        mPos = 0;
    }
    
    size_t getMaxLen() const
    {
        return mMaxLen;
    }

    bool isAutoGrow() const
    {
        return mIsAutoMalloc;
    }

    void    setOP(PACKET_OP_TYPE op)
    {
        assert(mPos == 0);
        if (mPos == 0)
        {
            this->operator<< (static_cast<PACKET_LEN_TYPE>(PACKET_HEAD_LEN));
            this->operator<< (op);
        }
    }
    void    writeBool(bool value)
    {
        static_assert(sizeof(bool) == sizeof(int8_t), "");
        writeBuffer((char*)&value, sizeof(value));
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
        writeUINT32(binary.size());
        writeBuffer(binary.c_str(), binary.size());
    }
    void    writeBinary(const char* binary, size_t binaryLen)
    {
        writeUINT32(binaryLen);
        writeBuffer(binary, binaryLen);
    }

    void    claimBinary(const char* &binary, size_t binaryLen)
    {
        growBuffer(binaryLen+sizeof(int32_t));
        assert((mPos + binaryLen + sizeof(int32_t)) <= mMaxLen);
        if ((mPos + binaryLen + sizeof(int32_t)) <= mMaxLen)
        {
            writeUINT32(binaryLen);
            binary = getData()+mPos;
            mPos += binaryLen;
        }
        else
        {
            writeUINT32(0);
            binary = nullptr;
        }
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
    Packet & operator << (const bool &v)
    {
        writeBool(v);
        return *this;
    }
    Packet & operator << (const uint8_t &v)
    {
        writeUINT8(v);
        return *this;
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
    Packet & operator << (const uint16_t &v)
    {
        writeUINT16(v);
        return *this;
    }
    Packet & operator << (const int32_t &v)
    {
        writeINT32(v);
        return *this;
    }
    Packet & operator << (const uint32_t &v)
    {
        writeUINT32(v);
        return *this;
    }
    Packet & operator << (const int64_t &v)
    {
        writeINT64(v);
        return *this;
    }
    Packet & operator << (const uint64_t &v)
    {
        writeUINT64(v);
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
        static_assert(!std::is_pointer<T>::value, "T must is't a pointer");
        static_assert(std::is_class <T>::value, "T must a class or struct type");
        static_assert(std::is_pod <T>::value, "T must a pod type");
        writeBuffer((const char*)&v, sizeof(v));
        return *this;
    }

    size_t      getLen()
    {
        end();
        return mPos;
    }

    const char* getData()
    {
        return mBuffer;
    }

private:
    /*直接写入二进制流，消息包里不记录其长度*/
    void    writeBuffer(const char* buffer, size_t len)
    {
        growBuffer(len);

        if ((mPos + len) <= mMaxLen)
        {
            memcpy(mBuffer + mPos, buffer, len);
            mPos += len;
        }
        else
        {
            assert((mPos + len) <= mMaxLen);
        }
    }

    void        growBuffer(size_t len)
    {
        if (mIsAutoMalloc && (mPos + len) > mMaxLen)
        {
            char* oldMallocBuffer = mMallocBuffer;

            mMallocBuffer = (char*)malloc(mMaxLen + len);
            memcpy(mMallocBuffer, mBuffer, mPos);

            if (oldMallocBuffer != nullptr)
            {
                free(oldMallocBuffer);
                oldMallocBuffer = nullptr;
            }

            mMaxLen += len;
            mBuffer = mMallocBuffer;
        }
    }

    void        end()
    {
        PACKET_LEN_TYPE len = socketendian::hostToNetwork32(mPos);
        if (sizeof(len) <= mMaxLen)
        {
            memcpy(mBuffer, &len, sizeof(len));
            mIsFinish = true;
        }
        else
        {
            assert(false);
        }
    }
private:
    bool        mIsFinish;
    size_t      mPos;
    size_t      mMaxLen;
    char*       mBuffer;
    const bool  mIsAutoMalloc;
    char*       mMallocBuffer;
};

class ReadPacket
{
public:
    ReadPacket(const char* buffer, size_t len) : mMaxLen(len)
    {
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

    PACKET_LEN_TYPE readPacketLen()
    {
        return readUINT32();
    }

    PACKET_OP_TYPE  readOP()
    {
        return readUINT16();
    }

    bool        readBool()
    {
        static_assert(sizeof(bool) == sizeof(int8_t), "");
        bool  value = false;
        read(value);
        return value;
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
        size_t len = readUINT32();
        if ((mPos + len) <= mMaxLen)
        {
            ret = std::string((const char*)(mBuffer + mPos), len);
            mPos += len;
        }
        
        return ret;
    }
    /*  调用方不可管理str内存，只能读操作*/
    bool            readBinary(const char*& str, size_t& outLen)
    {
        bool ret = false;
        size_t len = readUINT32();
        outLen = len;
        if ((mPos + len) <= mMaxLen && len > 0)
        {
            str = mBuffer + mPos;
            mPos += len;
            ret = true;
        }

        return ret;
    }
    
    /*读取某自定义类型*/
    template<typename T>
    void           read(T& value)
    {
        static_assert(std::is_same < T, typename std::remove_pointer<T>::type>::value, "T must a nomal type");
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

    size_t          getPos() const
    {
        return mPos;
    }

    size_t          getMaxPos() const
    {
        return mMaxLen;
    }
private:
    size_t          mPos;
    const size_t    mMaxLen;
    const char*     mBuffer;
};

class SendPacket : public Packet
{
public:
    SendPacket(PACKET_OP_TYPE op, char* buffer, size_t len, bool isAutoMalloc = false) : Packet(buffer, len, isAutoMalloc)
    {
        setOP(op);
    }
};

template<size_t SIZE>
class FixedPacket : public SendPacket
{
public:
    FixedPacket(PACKET_OP_TYPE op) : SendPacket(op, mData, SIZE)
    {}
    
private:
    char        mData[SIZE];
};

template<size_t SIZE>
class AutoMallocPacket : public SendPacket
{
public:
    AutoMallocPacket(PACKET_OP_TYPE op) : SendPacket(op, mData, SIZE, true)
    {}
private:
    char        mData[SIZE];
};

typedef FixedPacket<128>        TinyPacket;
typedef FixedPacket<256>        ShortPacket;
typedef FixedPacket<512>        MiddlePacket;
typedef FixedPacket<1024>       LongPacket;
typedef AutoMallocPacket<32 * 1024>    BigPacket;


template<typename T>
static bool serializePBMsgToPacket(T& pbObj, Packet& packet)
{
    bool ret = false;
    int pbByteSize = pbObj.ByteSize();
    const char* buff = nullptr;

    packet.claimBinary(buff, pbByteSize);
    assert(buff != nullptr);
    if (buff != nullptr)
    {
        ret = pbObj.SerializeToArray((void*)buff, pbByteSize);
    }

    return ret;
}

template<typename T>
static bool deserializePBMsgToPacket(T& pbObj, ReadPacket& packet)
{
    const char* data = nullptr;
    size_t outLen = 0;
    packet.readBinary(data, outLen);
    assert(data != nullptr);
    if (data != nullptr)
    {
        pbObj.ParseFromArray(data, outLen);
        return true;
    }

    return false;
}

#endif