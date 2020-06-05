#pragma once

#include <cstdint>
#include <cassert>
#include <cstdbool>
#include <cstring>
#include <string>

#include <brynet/base/NonCopyable.hpp>
#include <brynet/base/endian/Endian.hpp>

namespace brynet { namespace base {

    class BasePacketWriter : public NonCopyable
    {
    public:
        BasePacketWriter(char* buffer,
            size_t len,
            bool useBigEndian = true,
            bool isAutoMalloc = false)
            :
            mIsAutoMalloc(isAutoMalloc),
            mBigEndian(useBigEndian)
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

        size_t      getPos() const
        {
            return mPos;
        }

        const char* getData() const
        {
            return mBuffer;
        }

        bool    isAutoGrow() const
        {
            return mIsAutoMalloc;
        }

        bool    writeBool(bool value)
        {
            static_assert(sizeof(bool) == sizeof(int8_t), "");
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeINT8(int8_t value)
        {
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeUINT8(uint8_t value)
        {
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeINT16(int16_t value)
        {
            value = endian::hostToNetwork16(value, mBigEndian);
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeUINT16(uint16_t value)
        {
            value = endian::hostToNetwork16(value, mBigEndian);
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeINT32(int32_t value)
        {
            value = endian::hostToNetwork32(value, mBigEndian);
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeUINT32(uint32_t value)
        {
            value = endian::hostToNetwork32(value, mBigEndian);
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeINT64(int64_t value)
        {
            value = endian::hostToNetwork64(value, mBigEndian);
            return writeBuffer((char*)&value, sizeof(value));
        }
        bool    writeUINT64(uint64_t value)
        {
            value = endian::hostToNetwork64(value, mBigEndian);
            return writeBuffer((char*)&value, sizeof(value));
        }

        bool    writeBinary(const std::string& binary)
        {
            return writeBuffer(binary.c_str(), binary.size());
        }
        bool    writeBinary(const char* binary, size_t binaryLen)
        {
            return writeBuffer(binary, binaryLen);
        }
        bool    writeBuffer(const char* buffer, size_t len)
        {
            growBuffer(len);

            if (mMaxLen < (mPos + len))
            {
                return false;
            }

            memcpy(mBuffer + mPos, buffer, len);
            mPos += len;
            return true;
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

    private:
        // 为了避免直接<<导致没有指定字节序导致隐藏BUG,因为此函数设置为私有
        template<typename T>
        BasePacketWriter & operator << (const T& v)
        {
            static_assert(!std::is_pointer<T>::value, "T must is't a pointer");
            static_assert(std::is_class <T>::value, "T must a class or struct type");
            static_assert(std::is_pod <T>::value, "T must a pod type");
            writeBuffer((const char*)&v, sizeof(v));
            return *this;
        }

        template<typename Arg1, typename... Args>
        void            writev(const Arg1& arg1, const Args&... args)
        {
            this->operator<<(arg1);
            writev(args...);
        }

        void            writev()
        {
        }

    protected:
        void        growBuffer(size_t len)
        {
            if (!mIsAutoMalloc || (mPos + len) <= mMaxLen)
            {
                return;
            }

            auto newBuffer = (char*)malloc(mMaxLen + len);
            if (newBuffer == nullptr)
            {
                return;
            }

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

    protected:
        const bool  mIsAutoMalloc;
        bool        mBigEndian;
        size_t      mPos;
        size_t      mMaxLen;
        char*       mBuffer;
        char*       mMallocBuffer;
    };

    class BasePacketReader
    {
    public:
        BasePacketReader(const char* buffer,
            size_t len,
            bool useBigEndian = true) :
            mBigEndian(useBigEndian),
            mMaxLen(len)
        {
            mPos = 0;
            mBuffer = buffer;
        }

        virtual ~BasePacketReader() = default;

        size_t          getLeft() const
        {
            if (mPos > mMaxLen)
            {
                throw std::out_of_range("current pos is greater than max len");
            }
            return mMaxLen - mPos;
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

        void            addPos(size_t diff)
        {
            const auto tmpPos = mPos + diff;
            if (tmpPos > mMaxLen)
            {
                throw std::out_of_range("diff is to big");
            }

            mPos = tmpPos;
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
            return endian::networkToHost16(value, mBigEndian);
        }
        uint16_t        readUINT16()
        {
            uint16_t  value = 0;
            read(value);
            return endian::networkToHost16(value, mBigEndian);
        }
        int32_t         readINT32()
        {
            int32_t  value = 0;
            read(value);
            return endian::networkToHost32(value, mBigEndian);
        }
        uint32_t        readUINT32()
        {
            uint32_t  value = 0;
            read(value);
            return endian::networkToHost32(value, mBigEndian);
        }
        int64_t         readINT64()
        {
            int64_t  value = 0;
            read(value);
            return endian::networkToHost64(value, mBigEndian);
        }
        uint64_t        readUINT64()
        {
            uint64_t  value = 0;
            read(value);
            return endian::networkToHost64(value, mBigEndian);
        }

    private:
        // 为了避免直接read(uintXXX)导致没有指定字节序造成隐患BUG,因为此函数设置为私有
        template<typename T>
        void            read(T& value)
        {
            static_assert(std::is_same<T, typename std::remove_pointer<T>::type>::value,
                "T must a nomal type");
            static_assert(std::is_pod<T>::value,
                "T must a pod type");

            if ((mPos + sizeof(value)) > mMaxLen)
            {
                throw std::out_of_range("T size is to big");
            }

            value = *(T*)(mBuffer + mPos);
            mPos += sizeof(value);
        }

    protected:
        const bool      mBigEndian;
        const size_t    mMaxLen;
        const char*     mBuffer;
        size_t          mPos;
    };

    template<size_t SIZE>
    class AutoMallocPacket : public BasePacketWriter
    {
    public:
        explicit AutoMallocPacket(bool useBigEndian = true,
            bool isAutoMalloc = false)
            :
            BasePacketWriter(mData, SIZE, useBigEndian, isAutoMalloc)
        {}
    private:
        char        mData[SIZE];
    };

    using BigPacket = AutoMallocPacket<32 * 1024>;

} }