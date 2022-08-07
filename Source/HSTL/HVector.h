// Created by mooming.go@gmail.com, 2017

#pragma once

#include <vector>

#include "Memory/BaseAllocator.h"
#include "Memory/ScopedAllocator.h"


namespace HSTD
{
    template <typename T>
    using Vector = std::vector<T, HE::BaseAllocator<T>>;

    template <typename T>
    using ScopedVector = std::basic_string<char, std::char_traits<char>, HE::ScopedAllocator<char>>;
}
