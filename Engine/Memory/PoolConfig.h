// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>

namespace hbe
{

/// @brief Configuration for a memory pool block.
class PoolConfig final
{
public:
	size_t blockSize;
	size_t numberOfBlocks;

	PoolConfig() noexcept : blockSize(0), numberOfBlocks(0) {}

	PoolConfig(size_t blockSize, size_t nBlocks) noexcept : blockSize(blockSize), numberOfBlocks(nBlocks) {}

	bool operator<(const PoolConfig& rhs) const noexcept { return blockSize < rhs.blockSize; }
};

} // namespace hbe
