// Created by mooming.go@gmail.com, 2022

#pragma once

#include "EndLine.h"
#include "StaticString.h"
#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include <cstdio>
#include <string>


namespace HE
{

template <class TCh = char, class TAlloc = BaseAllocator<TCh>>
class StringBuilder final
{
public:
    static constexpr int InlineBufferSize = 64;
    
    using This = StringBuilder;
    using TString = std::basic_string<TCh, std::char_traits<TCh>, TAlloc>;
    
private:
    TString buffer;
    
public:
    StringBuilder() = default;
    ~StringBuilder() = default;
    
    void Reserve(size_t size)
    {
        buffer.reserve(size);
    }
    
    void Clear()
    {
        buffer.clear();
    }
    
    auto c_str() const
    {
        return buffer.c_str();
    }
    
    operator const TCh* () const
    {
        return buffer.c_str();
    }
    
    This& operator<< (nullptr_t)
    {
        buffer.append("Null");
        return *this;
    }
    
    This& operator<< (bool value)
    {
        buffer.append(value ? "True" : "False");
        return *this;
    }
    
    This& operator<< (char ch)
    {
        buffer.push_back(ch);
        return *this;
    }
    
    This& operator<< (unsigned char value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%d", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (const char* str)
    {
        buffer.append(str);
        return *this;
    }
    
    This& operator<< (StaticString str)
    {
        buffer.append(str.c_str());
        return *this;
    }
    
    This& operator<< (short value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%d", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (unsigned short value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%d", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (int value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%d", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (unsigned int value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%u", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%ld", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (unsigned long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%lu", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (long long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%lld", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (unsigned long long value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%llu", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (float value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%f", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (double value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%lf", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (long double value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%Lf", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (void* value)
    {
        char temp[InlineBufferSize];
        snprintf(temp, 64, "%p", value);
        buffer.append(temp);
        return *this;
    }
    
    This& operator<< (EndLine)
    {
        buffer.append("\n");
        return *this;
    }
};

template<int BufferSize = 256>
using InlineStringBuilder = StringBuilder<char, InlinePoolAllocator<char, BufferSize>>;
} // HSTL
