// Created by mooming.go@gmail.com, 2022

#pragma once
#include <cstddef>


struct PoolConfig final
{
    size_t blockSize = 0;
    size_t numberOfBlocks = 0;

    inline PoolConfig() : blockSize(0), numberOfBlocks(0) {}

    inline PoolConfig(size_t blockSize, size_t nBlocks)
        : blockSize(blockSize), numberOfBlocks(nBlocks)
    {
    }

    inline bool operator<(const PoolConfig& rhs) const { return blockSize < rhs.blockSize; }
};
