// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include <sstream>


namespace HSTL
{

using HStringStream = std::basic_stringstream<char,std::char_traits<char>, HE::BaseAllocator<char>>;

template <size_t PoolSize = 2048>
using HInlineStringStream = std::basic_stringstream<char,std::char_traits<char>, HE::InlinePoolAllocator<char, PoolSize, 2>>;

} // HSTL
