// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once
#include <cstddef>

/// @brief Configuration for a memory pool block.
struct PoolConfig final
{
	size_t blockSize = 0;
	size_t numberOfBlocks = 0;

	inline PoolConfig() : blockSize(0), numberOfBlocks(0) {}

	inline PoolConfig(size_t blockSize, size_t nBlocks) : blockSize(blockSize), numberOfBlocks(nBlocks) {}

	inline bool operator<(const PoolConfig& rhs) const { return blockSize < rhs.blockSize; }
};
