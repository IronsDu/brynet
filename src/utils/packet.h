#ifndef _PACKET_H
#define _PACKET_H

#include <stdint.h>
#include <assert.h>
#include <string>
#include <stdbool.h>
#include <string.h>

#include "SocketLibTypes.h"

#ifndef PLATFORM_WINDOWS
#include <endian.h>
#endif

typedef  uint32_t PACKET_LEN_TYPE;
typedef  uint16_t PACKET_OP_TYPE;

const static PACKET_LEN_TYPE PACKET_HEAD_LEN = sizeof(PACKET_LEN_TYPE) + sizeof(PACKET_OP_TYPE);

namespace socketendian
{
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
#else
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
#endif
}

class BasePacketWriter
{
public:
    BasePacketWriter(char* buffer, size_t len, bool useBigEndian = true, bool isAutoMalloc = false) : mBigEndian(useBigEndian), mIsAutoMalloc(isAutoMalloc)
    {
        mMaxLen = len;
        mPos = 0;
        mBuffer = buffer;
        mMallocBuffer = nullptr;
    }

    virtual ~BasePacketWriter()
    {
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

    size_t      getPos()
    {
        return mPos;
    }

    const char* getData()
    {
        return mBuffer;
    }

    bool isAutoGrow() const
    {
        return mIsAutoMalloc;
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
        value = socketendian::hostToNetwork16(value, mBigEndian);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT16(uint16_t value)
    {
        value = socketendian::hostToNetwork16(value, mBigEndian);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeINT32(int32_t value)
    {
        value = socketendian::hostToNetwork32(value, mBigEndian);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT32(uint32_t value)
    {
        value = socketendian::hostToNetwork32(value, mBigEndian);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeINT64(int64_t value)
    {
        value = socketendian::hostToNetwork64(value, mBigEndian);
        writeBuffer((char*)&value, sizeof(value));
    }
    void    writeUINT64(uint64_t value)
    {
        value = socketendian::hostToNetwork64(value, mBigEndian);
        writeBuffer((char*)&value, sizeof(value));
    }

    void    writeBinary(const std::string& binary)
    {
        writeBuffer(binary.c_str(), binary.size());
    }
    void    writeBinary(const char* binary, size_t binaryLen)
    {
        writeBuffer(binary, binaryLen);
    }
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
    BasePacketWriter & operator << (const bool &v)
    {
        writeBool(v);
        return *this;
    }
    BasePacketWriter & operator << (const uint8_t &v)
    {
        writeUINT8(v);
        return *this;
    }
    BasePacketWriter & operator << (const int8_t &v)
    {
        writeINT8(v);
        return *this;
    }
    BasePacketWriter & operator << (const int16_t &v)
    {
        writeINT16(v);
        return *this;
    }
    BasePacketWriter & operator << (const uint16_t &v)
    {
        writeUINT16(v);
        return *this;
    }
    BasePacketWriter & operator << (const int32_t &v)
    {
        writeINT32(v);
        return *this;
    }
    BasePacketWriter & operator << (const uint32_t &v)
    {
        writeUINT32(v);
        return *this;
    }
    BasePacketWriter & operator << (const int64_t &v)
    {
        writeINT64(v);
        return *this;
    }
    BasePacketWriter & operator << (const uint64_t &v)
    {
        writeUINT64(v);
        return *this;
    }
    BasePacketWriter & operator << (const char* const &v)
    {
        writeBinary(v);
        return *this;
    }
    BasePacketWriter & operator << (const std::string &v)
    {
        writeBinary(v);
        return *this;
    }
    /*写入自定义类型对象的内存区，消息包里不记录其长度，反序列方则使用ReadPacket::read<T>(v)进行读取*/
    template<typename T>
    BasePacketWriter & operator << (const T& v)
    {
        static_assert(!std::is_pointer<T>::value, "T must is't a pointer");
        static_assert(std::is_class <T>::value, "T must a class or struct type");
        static_assert(std::is_pod <T>::value, "T must a pod type");
        writeBuffer((const char*)&v, sizeof(v));
        return *this;
    }

protected:
    void        growBuffer(size_t len)
    {
        if (mIsAutoMalloc && (mPos + len) > mMaxLen)
        {
            char* newBuffer = (char*)malloc(mMaxLen + len);
            if (newBuffer != nullptr)
            {
                memcpy(newBuffer, mBuffer, mPos);

                if (mMallocBuffer != nullptr)
                {
                    free(mMallocBuffer);
                    mMallocBuffer = nullptr;
                }

                mMaxLen += len;
                mMallocBuffer = newBuffer;
                mBuffer = newBuffer;
            }
        }
    }

protected:
    bool        mBigEndian;
    size_t      mPos;
    size_t      mMaxLen;
    char*       mBuffer;
    const bool  mIsAutoMalloc;
    char*       mMallocBuffer;
};

/*  使用传统二进制封包协议的Packet Writer   */
class Packet : public BasePacketWriter
{
public:
    Packet(char* buffer, size_t len, bool useBigEndian = true, bool isAutoMalloc = false) : BasePacketWriter(buffer, len, useBigEndian, isAutoMalloc)
    {
        mIsFinish = false;
    }
    
    virtual ~Packet()
    {
        assert(mIsFinish);
    }

    void    init()
    {
        BasePacketWriter::init();
        mIsFinish = false;
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

    /*写入二进制字符串，并记录其长度，反序列方使用ReadPacket::readBinary()读取*/
    void    writeBinary(const std::string& binary)
    {
        writeUINT32(static_cast<uint32_t>(binary.size()));
        writeBuffer(binary.c_str(), binary.size());
    }
    void    writeBinary(const char* binary, size_t binaryLen)
    {
        writeUINT32(static_cast<uint32_t>(binaryLen));
        writeBuffer(binary, binaryLen);
    }

    void    claimBinary(const char* &binary, size_t binaryLen)
    {
        growBuffer(binaryLen+sizeof(int32_t));
        assert((mPos + binaryLen + sizeof(int32_t)) <= mMaxLen);
        if ((mPos + binaryLen + sizeof(int32_t)) <= mMaxLen)
        {
            writeUINT32(static_cast<uint32_t>(binaryLen));
            binary = getData()+mPos;
            mPos += binaryLen;
        }
        else
        {
            writeUINT32(0);
            binary = nullptr;
        }
    }

    size_t      getLen()
    {
        end();
        return getPos();
    }

private:
    void        end()
    {
        if (!mIsFinish)
        {
            PACKET_LEN_TYPE len = socketendian::hostToNetwork32(static_cast<uint32_t>(mPos));
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
    }

private:
    bool        mIsFinish;
};

class BasePacketReader
{
public:
    BasePacketReader(const char* buffer, size_t len, bool useBigEndian = true) : mMaxLen(len), mBigEndian(useBigEndian)
    {
        mPos = 0;
        mBuffer = buffer;
    }

    virtual ~BasePacketReader()
    {
        if (mPos != mMaxLen)
        {
            assert(mPos == mMaxLen);
        }
    }

    const char*     getBuffer() const
    {
        return mBuffer;
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

    void            addPos(int value)
    {
        auto tmp = mPos + value;
        if (tmp >= 0 && tmp <= mMaxLen)
        {
            mPos = tmp;
        }
    }

    bool            readBool()
    {
        static_assert(sizeof(bool) == sizeof(int8_t), "");
        bool  value = false;
        read(value);
        return value;
    }
    int8_t          readINT8()
    {
        int8_t  value = 0;
        read(value);
        return value;
    }
    uint8_t         readUINT8()
    {
        uint8_t  value = 0;
        read(value);
        return value;
    }
    int16_t         readINT16()
    {
        int16_t  value = 0;
        read(value);
        return socketendian::networkToHost16(value, mBigEndian);
    }
    uint16_t        readUINT16()
    {
        uint16_t  value = 0;
        read(value);
        return socketendian::networkToHost16(value, mBigEndian);
    }
    int32_t         readINT32()
    {
        int32_t  value = 0;
        read(value);
        return socketendian::networkToHost32(value, mBigEndian);
    }
    uint32_t        readUINT32()
    {
        uint32_t  value = 0;
        read(value);
        return socketendian::networkToHost32(value, mBigEndian);
    }
    int64_t         readINT64()
    {
        int64_t  value = 0;
        read(value);
        return socketendian::networkToHost64(value, mBigEndian);
    }
    uint64_t        readUINT64()
    {
        uint64_t  value = 0;
        read(value);
        return socketendian::networkToHost64(value, mBigEndian);
    }

    /*读取某自定义类型*/
    template<typename T>
    void            read(T& value)
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

protected:
    bool            mBigEndian;
    size_t          mPos;
    const size_t    mMaxLen;
    const char*     mBuffer;
};

/*  使用传统二进制封包协议的packet reader   */
class ReadPacket : public BasePacketReader
{
public:
    ReadPacket(const char* buffer, size_t len, bool useBigendian = true) : BasePacketReader(buffer, len, useBigendian)
    {
    }

    PACKET_LEN_TYPE readPacketLen()
    {
        return readUINT32();
    }

    PACKET_OP_TYPE  readOP()
    {
        return readUINT16();
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
};

class SendPacket : public Packet
{
public:
    SendPacket(PACKET_OP_TYPE op, char* buffer, size_t len, bool useBigEndian = true, bool isAutoMalloc = false) : Packet(buffer, len, useBigEndian, isAutoMalloc)
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
        ret = pbObj.SerializePartialToArray((void*)buff, pbByteSize);
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
        pbObj.ParsePartialFromArray(data, outLen);
        return true;
    }

    return false;
}

#endif
