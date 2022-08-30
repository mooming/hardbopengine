// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include <sstream>


namespace HSTL
{

using HStringStream = std::basic_stringstream<char,std::char_traits<char>, HE::BaseAllocator<char>>;

} // HSTL
