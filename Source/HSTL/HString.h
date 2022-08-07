// Created by mooming.go@gmail.com, 2017

#pragma once

#include <string>

#include "Memory/BaseAllocator.h"
#include "Memory/ScopedAllocator.h"


namespace HSTD
{
    using TString = std::basic_string<char, std::char_traits<char>, HE::BaseAllocator<char>>;
    using TScopedString = std::basic_string<char, std::char_traits<char>, HE::ScopedAllocator<char>>;
}
