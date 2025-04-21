// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include <string>

namespace HSTL
{
using HString =
    std::basic_string<char, std::char_traits<char>, HE::BaseAllocator<char>>;

template <size_t PoolSize = 128>
using HInlineString = std::basic_string<char, std::char_traits<char>,
    HE::InlinePoolAllocator<char, PoolSize, 2>>;
using HInlinePathString = std::basic_string<char, std::char_traits<char>,
    HE::InlinePoolAllocator<char, HE::Config::MaxPathLength, 2>>;
} // namespace HSTL

namespace std
{
template <>
struct hash<HSTL::HString> final
{
    std::size_t operator()(const HSTL::HString& obj) const
    {
        auto text = obj.c_str();
        size_t hashCode = 5381;

        while (*text != '\0')
        {
            size_t ch = *text;
            ++text;
            hashCode = ((hashCode << 5) + hashCode) + ch;
        }

        return hashCode;
    }
};
} // namespace std
