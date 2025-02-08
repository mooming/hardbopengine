// Created by mooming.go@gmail.com, 2022

#pragma once

#include "EndLine.h"
#include "OSAL/Intrinsic.h"
#include "StaticString.h"
#include "StringUtil.h"
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace HE
{

    /**
     InlineStringBuilder
     A buffer designed for String Building.
     It declares a fixed size static buffer. It doesn't handle a logner text
     than its buffer size. It'll drop letters exceeding its limit.
     */
    template <size_t BufferSize = 1024, class TChar = char>
    class InlineStringBuilder final
    {
    private:
        static_assert(BufferSize > 0, "BufferSize should be greater than 0.");
        static constexpr size_t LastIndex = BufferSize - 1;
        using This = InlineStringBuilder;

    private:
        size_t length;
        TChar buffer[BufferSize];

    public:
        InlineStringBuilder()
            : length(0)
        {
            buffer[0] = '\0';
            buffer[LastIndex] = '\0';
        }

        ~InlineStringBuilder() = default;

        void Clear()
        {
            length = 0;
            buffer[0] = '\0';
            buffer[LastIndex] = '\0';
        }

        auto c_str() const { return static_cast<const char *>(buffer); }

        auto Size() const { return length; }

        operator const TChar *() const { return c_str(); }

        This &Hex(uint8_t value)
        {
            auto print = [&]() {
                const size_t remained = BufferSize - length;

                if (std::isprint(value))
                {
                    return snprintf(&buffer[length], remained, "%c", value);
                }

                return snprintf(&buffer[length], remained, "%02x", value);
            };

            int written = print();

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(nullptr_t) { return *this << "Null"; }

        This &operator<<(bool value)
        {
            if (value)
            {
                return *this << "True";
            }

            return *this << "False";
        }

        This &operator<<(char ch)
        {
            if (length >= LastIndex)
            {
                return *this;
            }

            buffer[length++] = ch;

            Assert(length <= LastIndex);
            buffer[length] = '\0';

            return *this;
        }

        This &operator<<(unsigned char value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%u", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(const char *str)
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
            {
                return *this;
            }

            memcpy((void *)(&buffer[length]), str, len);
            length = newLength;
            buffer[length] = '\0';

            return *this;
        }

        template <size_t N>
        This &operator<<(char str[N])
        {
            if (N == 0)
            {
                return *this;
            }

            auto len = StringUtil::StrLen(str, N);
            size_t newLength = length + len;

            if (unlikely(newLength >= LastIndex))
            {
                newLength = LastIndex;
                Assert(length <= LastIndex);
                len = LastIndex - length;
            }

            if (unlikely(len <= 0))
            {
                return *this;
            }

            memcpy((void *)(&buffer[length]), &str[0], len);
            length = newLength;
            buffer[length] = '\0';

            return *this;
        }

        This &operator<<(const std::string_view &str)
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
            {
                return *this;
            }

            memcpy((void *)(&buffer[length]), str.data(), len);

            length = newLength;
            buffer[length] = '\0';

            return *this;
        }

        template <class CharT, class Traits, class Allocator>
        This &operator<<(const std::basic_string<CharT, Traits, Allocator> &str)
        {
            return *this << static_cast<std::string_view>(str);
        }

        This &operator<<(StaticString str) { return *this << str.c_str(); }

        This &operator<<(short value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%d", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(unsigned short value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%u", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(int value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%d", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(unsigned int value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%u", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(long value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%ld", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(unsigned long value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%lu", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(long long value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%lld", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(unsigned long long value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%llu", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(float value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%f", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(double value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%lf", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(long double value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%Lf", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(void *value)
        {
            const size_t remained = BufferSize - length;
            auto written = snprintf(&buffer[length], remained, "%p", value);

            Assert(written >= 0);
            length = std::min(LastIndex, length + written);

            return *this;
        }

        This &operator<<(EndLine) { return *this << '\n'; }
    };

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
    class InlineStringBuilderTest : public TestCollection
    {
    public:
        InlineStringBuilderTest()
            : TestCollection("InlineStringBuilderTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };
} // namespace HE

#endif //__UNIT_TEST__
