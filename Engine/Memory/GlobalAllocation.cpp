// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "Log/Logger.h"
#include "MemoryManager.h"
#include "OSAL/OSMemory.h"

#if defined(__APPLE__)
extern "C" const void* _dyld_get_shared_cache_range(size_t* length);

#define HBE_ACCOUNTING_CALLER __builtin_return_address(0)
#else
#define HBE_ACCOUNTING_CALLER nullptr
#endif

namespace
{
std::atomic<size_t> globalAllocationBytes{0};
std::atomic<uint64_t> globalAllocationCount{0};

std::atomic<size_t> globalFreeBytes{0};
std::atomic<uint64_t> globalFreeCount{0};

std::atomic<size_t> globalOSAllocationBytes{0};
std::atomic<uint64_t> globalOSAllocationCount{0};

std::atomic<size_t> globalOSFreeBytes{0};
std::atomic<uint64_t> globalOSFreeCount{0};

std::atomic<uintptr_t> osImageRangeBase{0};
std::atomic<size_t> osImageRangeSize{0};
std::atomic<bool> osImageRangeQueried{false};

bool IsOSImageCaller(void* caller) noexcept
{
#if defined(__APPLE__)
	const uintptr_t address = reinterpret_cast<uintptr_t>(caller);

	if (!osImageRangeQueried.load(std::memory_order_acquire))
	{
		size_t sharedCacheSize = 0;
		const void* const sharedCacheBase = _dyld_get_shared_cache_range(&sharedCacheSize);

		osImageRangeBase.store(reinterpret_cast<uintptr_t>(sharedCacheBase), std::memory_order_relaxed);
		osImageRangeSize.store(sharedCacheSize, std::memory_order_relaxed);
		osImageRangeQueried.store(true, std::memory_order_release);
	}

	const uintptr_t base = osImageRangeBase.load(std::memory_order_relaxed);
	const size_t size = osImageRangeSize.load(std::memory_order_relaxed);

	return size != 0 && address - base < size;
#else
	(void) caller;

	return false;
#endif
}

void RecordAllocation(size_t size, void* caller) noexcept
{
	hbe::MemoryManager::RecordGlobalAllocation(size);

	if (IsOSImageCaller(caller))
	{
		globalOSAllocationBytes.fetch_add(size, std::memory_order_relaxed);
		globalOSAllocationCount.fetch_add(1, std::memory_order_relaxed);
	}
}

void RecordDeallocation(size_t size, void* caller) noexcept
{
	hbe::MemoryManager::RecordGlobalFree(size);

	if (IsOSImageCaller(caller))
	{
		globalOSFreeBytes.fetch_add(size, std::memory_order_relaxed);
		globalOSFreeCount.fetch_add(1, std::memory_order_relaxed);
	}
}

void ReportAllocationFailure(size_t size, std::align_val_t alignment) noexcept
{
	auto log = hbe::Logger::Get("Memory");

	log.OutError([size, alignment](auto& ls)
	{
		ls << "Global allocation of " << size << " bytes with alignment " << static_cast<size_t>(alignment)
		   << " failed with no new_handler installed. The engine is exception-free, so this aborts "
			  "instead of throwing bad_alloc - the alternative would lose the message with it.";
	});

	std::abort();
}

void* AllocateAccounted(size_t size, void* caller)
{
	const size_t requestSize = size == 0 ? 1 : size;

	while (true)
	{
		void* ptr = std::malloc(requestSize);
		if (ptr != nullptr)
		{
			RecordAllocation(size, caller);

			return ptr;
		}

		auto* handler = std::get_new_handler();
		if (handler == nullptr)
		{
			ReportAllocationFailure(size, static_cast<std::align_val_t>(alignof(std::max_align_t)));
		}

		handler();
	}
}

void* AllocateAccountedAligned(size_t size, std::align_val_t alignment, void* caller)
{
	const size_t requestSize = size == 0 ? 1 : size;
	const size_t requestAlignment = static_cast<size_t>(alignment);

	void* ptr = nullptr;

	if (requestAlignment <= alignof(std::max_align_t))
	{
		ptr = std::malloc(requestSize);
	}
	else
	{
		const size_t alignedSize = (requestSize + requestAlignment - 1) & ~(requestAlignment - 1);

		if (posix_memalign(&ptr, requestAlignment, alignedSize) != 0)
		{
			ptr = nullptr;
		}
	}

	if (ptr != nullptr)
	{
		RecordAllocation(size, caller);

		return ptr;
	}

	ReportAllocationFailure(size, alignment);

	return nullptr;
}

void* TryAllocateAccounted(size_t size, void* caller) noexcept
{
	void* ptr = std::malloc(size == 0 ? 1 : size);
	if (ptr != nullptr)
	{
		RecordAllocation(size, caller);
	}

	return ptr;
}

void* TryAllocateAccountedAligned(size_t size, std::align_val_t alignment, void* caller) noexcept
{
	const size_t requestSize = size == 0 ? 1 : size;
	const size_t requestAlignment = static_cast<size_t>(alignment);

	if (requestAlignment <= alignof(std::max_align_t))
	{
		return TryAllocateAccounted(size, caller);
	}

	void* ptr = nullptr;
	const size_t alignedSize = (requestSize + requestAlignment - 1) & ~(requestAlignment - 1);

	if (posix_memalign(&ptr, requestAlignment, alignedSize) != 0)
	{
		return nullptr;
	}

	RecordAllocation(size, caller);

	return ptr;
}

void Deallocate(void* ptr, size_t size, void* caller) noexcept
{
	if (ptr != nullptr)
	{
		RecordDeallocation(size != 0 ? size : OS::GetAllocSize(ptr), caller);
		std::free(ptr);
	}
}
} // namespace

namespace hbe
{
void MemoryManager::RecordGlobalAllocation(size_t nBytes) noexcept
{
	globalAllocationBytes.fetch_add(nBytes, std::memory_order_relaxed);
	globalAllocationCount.fetch_add(1, std::memory_order_relaxed);
}

void MemoryManager::RecordGlobalFree(size_t nBytes) noexcept
{
	globalFreeBytes.fetch_add(nBytes, std::memory_order_relaxed);
	globalFreeCount.fetch_add(1, std::memory_order_relaxed);
}

size_t MemoryManager::GetGlobalAllocationBytes() noexcept
{
	return globalAllocationBytes.load(std::memory_order_relaxed);
}

uint64_t MemoryManager::GetGlobalAllocationCount() noexcept
{
	return globalAllocationCount.load(std::memory_order_relaxed);
}

size_t MemoryManager::GetGlobalFreeBytes() noexcept
{
	return globalFreeBytes.load(std::memory_order_relaxed);
}

uint64_t MemoryManager::GetGlobalFreeCount() noexcept
{
	return globalFreeCount.load(std::memory_order_relaxed);
}

size_t MemoryManager::GetOSAllocationBytes() noexcept
{
	return globalOSAllocationBytes.load(std::memory_order_relaxed);
}

uint64_t MemoryManager::GetOSAllocationCount() noexcept
{
	return globalOSAllocationCount.load(std::memory_order_relaxed);
}

size_t MemoryManager::GetOSFreeBytes() noexcept
{
	return globalOSFreeBytes.load(std::memory_order_relaxed);
}

uint64_t MemoryManager::GetOSFreeCount() noexcept
{
	return globalOSFreeCount.load(std::memory_order_relaxed);
}
} // namespace hbe

void* operator new(size_t size)
{
	return AllocateAccounted(size, HBE_ACCOUNTING_CALLER);
}

void* operator new[](size_t size)
{
	return AllocateAccounted(size, HBE_ACCOUNTING_CALLER);
}

void* operator new(size_t size, std::align_val_t alignment)
{
	return AllocateAccountedAligned(size, alignment, HBE_ACCOUNTING_CALLER);
}

void* operator new[](size_t size, std::align_val_t alignment)
{
	return AllocateAccountedAligned(size, alignment, HBE_ACCOUNTING_CALLER);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
	return TryAllocateAccounted(size, HBE_ACCOUNTING_CALLER);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
	return TryAllocateAccounted(size, HBE_ACCOUNTING_CALLER);
}

void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return TryAllocateAccountedAligned(size, alignment, HBE_ACCOUNTING_CALLER);
}

void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return TryAllocateAccountedAligned(size, alignment, HBE_ACCOUNTING_CALLER);
}

void operator delete(void* ptr) noexcept
{
	Deallocate(ptr, 0, HBE_ACCOUNTING_CALLER);
}

void operator delete(void* ptr, size_t size) noexcept
{
	Deallocate(ptr, size, HBE_ACCOUNTING_CALLER);
}

void operator delete[](void* ptr) noexcept
{
	Deallocate(ptr, 0, HBE_ACCOUNTING_CALLER);
}

void operator delete[](void* ptr, size_t size) noexcept
{
	Deallocate(ptr, size, HBE_ACCOUNTING_CALLER);
}

void operator delete(void* ptr, std::align_val_t alignment) noexcept
{
	Deallocate(ptr, static_cast<size_t>(alignment), HBE_ACCOUNTING_CALLER);
}

void operator delete(void* ptr, size_t size, std::align_val_t) noexcept
{
	Deallocate(ptr, size, HBE_ACCOUNTING_CALLER);
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept
{
	Deallocate(ptr, static_cast<size_t>(alignment), HBE_ACCOUNTING_CALLER);
}

void operator delete[](void* ptr, size_t size, std::align_val_t) noexcept
{
	Deallocate(ptr, size, HBE_ACCOUNTING_CALLER);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
	Deallocate(ptr, 0, HBE_ACCOUNTING_CALLER);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
	Deallocate(ptr, 0, HBE_ACCOUNTING_CALLER);
}

#ifdef __TEST__
#include <string>
#include <vector>

namespace hbe
{
void GlobalAllocationTest::Prepare()
{
	AddTest("Engine traffic reaches the global counters", [this](auto& ls)
	{
		const auto totalBefore = MemoryManager::GetGlobalAllocationCount();

		std::vector<size_t> values;

		for (size_t i = 0; i < 64; ++i)
		{
			values.push_back(i);
		}

		const auto totalAfter = MemoryManager::GetGlobalAllocationCount();

		if (totalAfter <= totalBefore)
		{
			ls << "std::vector growth through operator new was not counted globally" << lferr;
		}
	});

#if defined(__APPLE__)
	AddTest("OS image traffic reaches its own bucket", [this](auto& ls)
	{
		const auto osRequestsBefore = MemoryManager::GetOSAllocationCount();
		const auto osBytesBefore = MemoryManager::GetOSAllocationBytes();
		const auto totalBefore = MemoryManager::GetGlobalAllocationCount();

		std::string grow;

		for (int i = 0; i < 4096; ++i)
		{
			grow.push_back('y');
		}

		const auto osRequestsAfter = MemoryManager::GetOSAllocationCount();
		const auto osBytesAfter = MemoryManager::GetOSAllocationBytes();
		const auto totalAfter = MemoryManager::GetGlobalAllocationCount();

		if (osRequestsAfter <= osRequestsBefore)
		{
			ls << "A libc++ reallocation inside the shared cache was not bucketed as OS image traffic" << lferr;
		}

		if (osBytesAfter - osBytesBefore < grow.size())
		{
			ls << "The OS bucket did not see the string growth bytes" << lferr;
		}

		if (totalAfter <= totalBefore)
		{
			ls << "OS image traffic bypassed the global total" << lferr;
		}
	});
#endif // defined(__APPLE__)
}
} // namespace hbe
#endif // __TEST__
