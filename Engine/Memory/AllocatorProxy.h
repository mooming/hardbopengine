// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <functional>
#include <thread>
#include "AllocStats.h"
#include "AllocatorID.h"
#include "Config/BuildConfig.h"

namespace hbe
{

	class AllocatorProxy final
	{
	public:
		using TAllocBytes = void* (*) (void* /*userData*/, size_t /* allocSize */);
		using TDeallocBytes = void (*)(void* /*userData*/, void* /* pointer */, size_t /* allocSize */);

		TAllocatorID id;
		AllocatorProxy* next;
		void* allocator;
		TAllocBytes allocate;
		TDeallocBytes deallocate;

#if PROFILE_ENABLED
		AllocStats stats;
#endif // PROFILE_ENABLED

#if MEMORY_VERIFICATION_ENABLED
		std::thread::id threadId;
#endif // MEMORY_VERIFICATION_ENABLED

	public:
		AllocatorProxy() : id(InvalidAllocatorID), next(nullptr), allocator(nullptr), allocate(nullptr), deallocate(nullptr) {}

#if PROFILE_ENABLED
		[[nodiscard]] const char* GetName() const { return stats.name; }
#else // PROFILE_ENABLED
		[[nodiscard]] const char* GetName() const { return "NoName"; }
#endif // PROFILE_ENABLED
	};

} // namespace hbe
