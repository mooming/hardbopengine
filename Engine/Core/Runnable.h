// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstdint>


namespace hbe
{
	// A function which performs a RangedTask. A runnable can return before it processes the entire indices.
	// @param userData
	// @param startIndex - RangedTask Start Index
	// @param endIndex - RangedTask End Index
	// @return Number of processed indices. If it doesn't finish the entire jobs, then this runnable will be called again with an updated startIndex.
	using Runnable = std::size_t(*)(void* /*userData*/, std::size_t /*startIndex*/, std::size_t /*endIndex*/);
} // namespace hbe
