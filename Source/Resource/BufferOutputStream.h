// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Buffer.h"
#include "HSTL/HString.h"
#include "System/Debug.h"
#include <cstddef>
#include <cstdint>
#include <thread>


namespace HE
{

class BufferOutputStream final
{
    using This = BufferOutputStream;

private:
    Buffer& buffer;
    size_t cursor;
    size_t errorCount;
    std::thread::id threadID;

public:
    BufferOutputStream(const BufferOutputStream&) = delete;
    BufferOutputStream(BufferOutputStream&&) = delete;

public:
    BufferOutputStream(Buffer& buffer);
    ~BufferOutputStream() = default;

    inline auto GetErrorCount() const { return errorCount; }
    inline bool HasError() const { return errorCount > 0; }
    inline void ClearErrorCount() { errorCount = 0; }
    inline bool IsDone() const { return cursor >= buffer.GetSize(); }
    
public:
    This& operator << (char value);
    This& operator << (int8_t value);
    This& operator << (uint8_t value);
    This& operator << (int16_t value);
    This& operator << (uint16_t value);
    This& operator << (int32_t value);
    This& operator << (uint32_t value);
    This& operator << (int64_t value);
    This& operator << (uint64_t value);
    This& operator << (float value);
    This& operator << (double value);
    This& operator << (long double value);
    This& operator << (const char* str);

    template <typename T, size_t N>
    This& operator << (T (&array)[N])
    {
        Put<T>(array, N);
        return *this;
    }

    template <size_t N>
    This& operator << (const HSTL::HInlineString<N>& str)
    {
        return *this << str.c_str();
    }

    inline This& operator << (const HSTL::HString& str)
    {
        return *this << str.c_str();
    }

    inline This& operator << (StaticString str)
    {
        return *this << str.c_str();
    }

private:
    inline bool IsValidIndex(size_t index) const
    {
        return cursor < buffer.GetSize();
    }

    template <typename T>
    void Put (T value)
    {
        Assert(std::this_thread::get_id() == threadID);

        constexpr size_t tSize = sizeof(T);
        const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
        const size_t size = buffer.GetSize();

        auto bufferBase = buffer.GetData();
        while (cursor < startIndex)
        {
            bufferBase[cursor++] = 0;

            if (cursor >= size)
            {
                ++errorCount;
                return;
            }
        }

        const auto newIndex = cursor + tSize;
        if (newIndex > size)
        {
            ++errorCount;
            return;
        }

        auto data = reinterpret_cast<T*>(&bufferBase[cursor]);
        data[0] = value;

        cursor = newIndex;
    }

    template <typename T>
    void Put (const T* value, size_t length)
    {
        Assert(std::this_thread::get_id() == threadID);
        
        Put<size_t>(length);
        
        constexpr size_t tSize = sizeof(T);
        const size_t startIndex = ((cursor + tSize - 1) / tSize) * tSize;
        const size_t size = buffer.GetSize();

        auto bufferBase = buffer.GetData();
        while (cursor < startIndex)
        {
            bufferBase[cursor++] = 0;

            if (cursor >= size)
            {
                ++errorCount;
                return;
            }
        }

        const auto newIndex = cursor + (tSize * length);
        if (newIndex > size)
        {
            ++errorCount;
            return;
        }

        auto data = reinterpret_cast<T*>(&bufferBase[cursor]);
        for (size_t i = 0; i < length; ++i)
        {
            data[i] = value[i];
        }

        cursor = newIndex;
    }
};

} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
class BufferOutputStreamTest : public TestCollection
{
public:
    BufferOutputStreamTest();
    virtual ~BufferOutputStreamTest() = default;

protected:
    virtual void Prepare() override;
};
} // HE
#endif //__UNIT_TEST__

