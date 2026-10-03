// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>


namespace hbe
{
// A function which performs a work item. A runnable can return before it processes the entire index range,
// @param userData
// @param startIndex - first index of the slice this call was given
// @param endIndex - one past the last index of that slice
// @return Number of processed indices. If it doesn't finish the entire jobs, then this
// runnable will be called again with an updated startIndex.
using TRunnable = std::size_t (*)(void* /*userData*/, std::size_t /*startIndex*/, std::size_t /*endIndex*/);
} // namespace hbe
