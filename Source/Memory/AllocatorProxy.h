// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Config/BuildConfig.h"
#include <atomic>
#include <thread>


namespace HE
{

struct AllocatorProxy final
{
    using TAllocBytes = std::function<void*(size_t)>;
    using TDeallocBytes = std::function<void(void*, size_t)>;

    static constexpr size_t NameBufferSize = 64;

    std::atomic<bool> isValid = false;
    
    TAllocBytes allocate = nullptr;
    TDeallocBytes deallocate = nullptr;

#ifdef __MEMORY_STATISTICS__
    bool isInline = false;
    bool hasCapacity = false;
    
    size_t capacity = 0;
    size_t usage = 0;
    size_t maxUsage = 0;
    size_t fallback = 0;

    size_t allocCount = 0;
    size_t deallocCount = 0;
    size_t fallbackCount = 0;

    char name[NameBufferSize] = "";
#endif // __MEMORY_STATISTICS__

#ifdef __MEMORY_VERIFICATION__
    std::thread::id threadId;
#endif // __MEMORY_VERIFICATION__
};

} // HE
