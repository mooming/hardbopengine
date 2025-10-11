// Created by mooming.go@gmail.com, 2022

#pragma once

#include <vector>
#include "Memory/DefaultAllocator.h"
#include "Memory/InlinePoolAllocator.h"

namespace hbe
{

	template<typename T>
	using HVector = std::vector<T, hbe::DefaultAllocator<T>>;

	template<typename T, size_t PoolSize = 16>
	using HInlineVector = std::vector<T, hbe::InlinePoolAllocator<T, PoolSize, 2>>;

} // namespace HSTL
