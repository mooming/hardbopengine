// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Config/BuildConfig.h"
#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include <string>


namespace HSTL
{
using HString = std::basic_string<char, std::char_traits<char>, HE::BaseAllocator<char>>;

template <size_t PoolSize = 128>
using HInlineString = std::basic_string<char, std::char_traits<char>, HE::InlinePoolAllocator<char, PoolSize, 2>>;
using HInlinePathString = std::basic_string<char, std::char_traits<char>, HE::InlinePoolAllocator<char, HE::Config::MaxPathLength, 2>>;
} // HSTL
