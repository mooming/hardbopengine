// Created by mooming.go@gmail.com, 2022

#pragma once

#include "EndLine.h"
#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include "StaticString.h"
#include <cstdio>
#include <string>
#include <string_view>


namespace HE
{

template <class TCh = char, class TAlloc = BaseAllocator<TCh>>
class StringBuilder final
{
  public:
    static constexpr int InlineBufferSize = 32;
    static constexpr int InlineFloatBufferSize = 64;
    static constexpr int InlineLongDoubleBufferSize = 512;

    using This = StringBuilder;
    using TString = std::basic_string<TCh, std::char_traits<TCh>, TAlloc>;

  private:
    TString buffer;

  public:
    StringBuilder() = default;
    ~StringBuilder() = default;

    void Reserve(size_t size) { buffer.reserve(size); }

    void Clear() { buffer.clear(); }

    auto c_str() const { return buffer.c_str(); }

    auto Size() const { return buffer.size(); }

    operator const TCh*() const { return buffer.c_str(); }

    This& operator<<(nullptr_t)
    {
        buffer.append("Null");
        return *this;
    }

    This& operator<<(bool value)
    {
        buffer.append(value ? "True" : "False");
        return *this;
    }

    This& operator<<(char ch)
    {
        buffer.push_back(ch);
        return *this;
    }

    This& operator<<(unsigned char value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%u", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(const char* str)
    {
        if (str == nullptr)
        {
            buffer.append("Null");
            return *this;
        }

        buffer.append(str);
        return *this;
    }

    This& operator<<(StaticString str)
    {
        buffer.append(str.c_str());
        return *this;
    }

    This& operator<<(const std::string_view& str)
    {
        buffer.append(str);

        return *this;
    }

    template <class CharT, class Traits, class Allocator>
    This& operator<<(const std::basic_string<CharT, Traits, Allocator>& str)
    {
        return *this << static_cast<std::string_view>(str);
    }

    This& operator<<(short value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%d", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(unsigned short value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%u", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(int value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%d", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(unsigned int value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%u", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%ld", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(unsigned long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%lu", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(long long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%lld", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(unsigned long long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%llu", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(float value)
    {
        char temp[InlineFloatBufferSize];
        snprintf(temp, InlineFloatBufferSize, "%f", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(double value)
    {
        char temp[InlineLongDoubleBufferSize];
        snprintf(temp, InlineLongDoubleBufferSize, "%lf", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(long double value)
    {
        char temp[InlineLongDoubleBufferSize];
        snprintf(temp, InlineLongDoubleBufferSize, "%Lf", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(void* value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, InlineBufferSize, "%p", value);
        buffer.append(temp);
        return *this;
    }

    This& operator<<(EndLine)
    {
        buffer.append("\n");
        return *this;
    }
};

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
class StringBuilderTest : public TestCollection
{
  public:
    StringBuilderTest() : TestCollection("StringBuilderTest") {}

  protected:
    virtual void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
