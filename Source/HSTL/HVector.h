// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include <vector>


namespace HSTL
{

template <typename T>
using HVector = std::vector<T, HE::BaseAllocator<T>>;

template <typename T, size_t PoolSize = 16>
using HInlineVector = std::vector<T, HE::InlinePoolAllocator<T, PoolSize, 2>>;

} // namespace HSTL
