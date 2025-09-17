// Created by mooming.go@gmail.com

#pragma once

#include <functional>
#include <thread>
#include "AllocStats.h"
#include "AllocatorID.h"
#include "Config/BuildConfig.h"

namespace hbe
{

	struct AllocatorProxy final
	{
		using TAllocBytes = void* (*) (void* /*userData*/, size_t /* allocSize */);
		using TDeallocBytes = void (*)(void* /*userData*/, void* /* pointer */, size_t /* allocSize */);

		TAllocatorID id = InvalidAllocatorID;
		AllocatorProxy* next = nullptr;
		void* allocator = nullptr;
		TAllocBytes allocate = nullptr;
		TDeallocBytes deallocate = nullptr;

#if PROFILE_ENABLED
		AllocStats stats;
#endif // PROFILE_ENABLED

#if __MEMORY_VERIFICATION__
		std::thread::id threadId;
#endif // __MEMORY_VERIFICATION__

#if PROFILE_ENABLED
		[[nodiscard]] const char* GetName() const { return stats.name; }
#else // PROFILE_ENABLED
		const char* GetName() const { return "NoName"; }
#endif // PROFILE_ENABLED
	};

} // namespace hbe
