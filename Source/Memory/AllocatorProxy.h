// Created by mooming.go@gmail.com

#pragma once

#include "AllocStats.h"
#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include <functional>
#include <thread>

namespace HE
{

    struct AllocatorProxy final
    {
        using TAllocBytes = std::function<void *(size_t)>;
        using TDeallocBytes = std::function<void(void *, size_t)>;

        TAllocatorID id = InvalidAllocatorID;
        AllocatorProxy *next = nullptr;
        TAllocBytes allocate = nullptr;
        TDeallocBytes deallocate = nullptr;

#ifdef PROFILE_ENABLED
        AllocStats stats;
#endif // PROFILE_ENABLED

#ifdef __MEMORY_VERIFICATION__
        std::thread::id threadId;
#endif // __MEMORY_VERIFICATION__

#ifdef PROFILE_ENABLED
        inline const char *GetName() const { return stats.name; }
#else  // PROFILE_ENABLED
        inline const char *GetName() const { return "NoName"; }
#endif // PROFILE_ENABLED
    };

} // namespace HE
