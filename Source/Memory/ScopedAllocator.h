// Created by mooming.go@gmail.com, 2022

#pragma once

#include "AllocatorScope.h"
#include "MemoryManager.h"
#include <memory>


namespace HE
{

template <typename TAlloc>
class ScopedAllocator final
{
private:
    TAlloc allocator;
    AllocatorScope scope;

public:
    ScopedAllocator(const ScopedAllocator&) = delete;
    ScopedAllocator(ScopedAllocator&&) = delete;
    ScopedAllocator& operator= (const ScopedAllocator&) = delete;
    ScopedAllocator& operator= (ScopedAllocator&&) = delete;

public:
    template <typename ... Types>
    ScopedAllocator(Types&& ... args)
        : allocator(std::forward<Types>(args) ...)
        , scope(allocator)
    {
    }

    ~ScopedAllocator() = default;

    auto& GetAllocator() { return allocator; }
    auto& GetAllocator() const { return allocator; }
};

} // HE
