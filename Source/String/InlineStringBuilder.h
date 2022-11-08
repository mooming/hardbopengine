// Created by mooming.go@gmail.com, 2022

#pragma once

#include "EndLine.h"
#include "StaticString.h"
#include "StringUtil.h"
#include "OSAL/Intrinsic.h"
#include <cstring>
#include <string>
#include <string_view>


namespace HE
{

template <size_t BufferSize = 1024, class TChar = char>
class InlineStringBuilder final
{
private:
    static_assert(BufferSize > 0, "BufferSize should be greater than 0.");

    static constexpr size_t LastIndex = BufferSize - 1;
    static constexpr int InternalBufferSize = 32;

    using This = InlineStringBuilder;

private:
    size_t length;
    TChar buffer[BufferSize];

public:
    InlineStringBuilder() : length(0)
    {
        buffer[0] = '\0';
    }

    ~InlineStringBuilder() = default;


    void Clear()
    {
        length = 0;
        buffer[0] = '\0';
    }

    auto c_str() const
    {
        return static_cast<const char*>(buffer);
    }

    auto Size() const
    {
        return length;
    }

    operator const TChar* () const
    {
        return c_str();
    }

    This& operator<< (nullptr_t)
    {
        return *this << "Null";
    }

    This& operator<< (bool value)
    {
        if (value)
        {
            return *this << "True";
        }

        return *this << "False";
    }

    This& operator<< (char ch)
    {
        if (length >= LastIndex)
            return *this;

        buffer[length++] = ch;
        return *this;
    }

    This& operator<< (unsigned char value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%u", value);

        return *this << temp;
    }

    This& operator<< (const char* str)
    {
        if (unlikely(str == nullptr))
        {
            return *this << nullptr;
        }

        auto len = StringUtil::StrLen(str);
        size_t newLength = length + len;

        if (unlikely(newLength >= LastIndex))
        {
            newLength = LastIndex;
            Assert(length <= LastIndex);
            len = LastIndex - length;
        }

        if (unlikely(len <= 0))
            return *this;

        memcpy((void*)(&buffer[length]), str, len);
        length = newLength;
        buffer[length] = '\0';

        return *this;
    }

    template <size_t N>
    This& operator<< (char str[N])
    {
        if (N == 0)
            return *this;
        
        auto len = StringUtil::StrLen(str, N);
        size_t newLength = length + len;

        if (unlikely(newLength >= LastIndex))
        {
            newLength = LastIndex;
            Assert(length <= LastIndex);
            len = LastIndex - length;
        }

        if (unlikely(len <= 0))
            return *this;

        memcpy((void*)(&buffer[length]), &str[0], len);
        length = newLength;
        buffer[length] = '\0';

        return *this;
    }

    This& operator<< (const std::string_view& str)
    {
        size_t len = str.length();
        size_t newLength = length + len;

        if (unlikely(newLength >= LastIndex))
        {
            newLength = LastIndex;
            Assert(length <= LastIndex);
            len = LastIndex - length;
        }

        if (unlikely(len <= 0))
            return *this;

        memcpy((void*)(&buffer[length]), str.data(), len);

        length = newLength;
        buffer[length] = '\0';

        return *this;
    }

    template <class CharT, class Traits, class Allocator>
    This& operator<< (const std::basic_string<CharT, Traits, Allocator>& str)
    {
        return *this << static_cast<std::string_view>(str);
    }

    This& operator<< (StaticString str)
    {
        return *this << str.c_str();
    }

    This& operator<< (short value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%d", value);

        return *this << temp;
    }

    This& operator<< (unsigned short value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%u", value);

        return *this << temp;
    }

    This& operator<< (int value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%d", value);

        return *this << temp;
    }

    This& operator<< (unsigned int value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%u", value);

        return *this << temp;
    }

    This& operator<< (long value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%ld", value);

        return *this << temp;
    }

    This& operator<< (unsigned long value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%lu", value);

        return *this << temp;
    }

    This& operator<< (long long value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%lld", value);

        return *this << temp;
    }

    This& operator<< (unsigned long long value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%llu", value);

        return *this << temp;
    }

    This& operator<< (float value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%f", value);

        return *this << temp;
    }

    This& operator<< (double value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%lf", value);

        return *this << temp;
    }

    This& operator<< (long double value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%Lf", value);

        return *this << temp;
    }

    This& operator<< (void* value)
    {
        char temp[InternalBufferSize];
        snprintf(temp, InternalBufferSize, "%p", value);

        return *this << temp;
    }

    This& operator<< (EndLine)
    {
        return *this << '\n';
    }
};

} // HSTL

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
    class InlineStringBuilderTest : public TestCollection
    {
    public:
        InlineStringBuilderTest() : TestCollection("InlineStringBuilderTest") {}

    protected:
        virtual void Prepare() override;
    };
} // HE

#endif //__UNIT_TEST__
