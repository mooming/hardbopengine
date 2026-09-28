// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <new>

#include "Log/Logger.h"
#include "MemoryManager.h"

/*
 * The engine's global allocation entry points, and the accounting that goes with them.
 *
 * Why the counters live here and not inside an AllocatorScope: a std::vector or std::string reaches the heap
 * through these entry points, which no scope can intercept, so an allocation made in the middle of a test can
 * otherwise happen entirely off the books. The counters are constant initialised atomics, which is what makes
 * them safe for the earliest static initialiser - there is no ready flag to get wrong, and no window where an
 * allocation goes unrecorded and its later release looks like an underflow. They are cumulative on both sides
 * for the same reason: nothing here pairs an allocation with its release, so an unrecorded pointer cannot drive
 * a negative figure.
 *
 * Why the backing store stays malloc: SystemAllocator is the owner of the system heap, and routing these entry
 * points through it is the right destination, but its investigation configuration allocates page granular with
 * underrun placement. Attaching every global allocation to that would silently turn each small std allocation
 * into a page and change what every configuration measures. So the decision taken here is to account first and
 * keep malloc, which also keeps the release path unambiguous - every pointer these produce is freeable by the
 * same call that was freeable before this file existed.
 *
 * On exhaustion this reports and aborts instead of throwing bad_alloc, after giving any installed new_handler
 * its turn. The engine is exception-free, so nothing could catch the exception anyway; aborting with a logged
 * size and alignment keeps the information that throwing would lose.
 */

namespace
{
std::atomic<size_t> globalAllocationBytes{0};
std::atomic<uint64_t> globalAllocationCount{0};

std::atomic<size_t> globalFreeBytes{0};
std::atomic<uint64_t> globalFreeCount{0};

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

void* AllocateAccounted(size_t size)
{
	const size_t requestSize = size == 0 ? 1 : size;

	while (true)
	{
		void* ptr = std::malloc(requestSize);
		if (ptr != nullptr)
		{
			hbe::MemoryManager::RecordGlobalAllocation(size);

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

void* AllocateAccountedAligned(size_t size, std::align_val_t alignment)
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
		hbe::MemoryManager::RecordGlobalAllocation(size);

		return ptr;
	}

	ReportAllocationFailure(size, alignment);

	return nullptr;
}

void* TryAllocateAccounted(size_t size) noexcept
{
	void* ptr = std::malloc(size == 0 ? 1 : size);
	if (ptr != nullptr)
	{
		hbe::MemoryManager::RecordGlobalAllocation(size);
	}

	return ptr;
}

void* TryAllocateAccountedAligned(size_t size, std::align_val_t alignment) noexcept
{
	const size_t requestSize = size == 0 ? 1 : size;
	const size_t requestAlignment = static_cast<size_t>(alignment);

	if (requestAlignment <= alignof(std::max_align_t))
	{
		return TryAllocateAccounted(size);
	}

	void* ptr = nullptr;
	const size_t alignedSize = (requestSize + requestAlignment - 1) & ~(requestAlignment - 1);

	if (posix_memalign(&ptr, requestAlignment, alignedSize) != 0)
	{
		return nullptr;
	}

	hbe::MemoryManager::RecordGlobalAllocation(size);

	return ptr;
}

void Deallocate(void* ptr, size_t size) noexcept
{
	if (ptr != nullptr)
	{
		hbe::MemoryManager::RecordGlobalFree(size);
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

} // namespace hbe

/*
 * Replacement allocation functions are global by language rule: a declaration of operator new inside a
 * namespace is rejected outright, so only the accounting above is able to carry the engine namespace.
 */

void* operator new(size_t size)
{
	return AllocateAccounted(size);
}

void* operator new[](size_t size)
{
	return AllocateAccounted(size);
}

void* operator new(size_t size, std::align_val_t alignment)
{
	return AllocateAccountedAligned(size, alignment);
}

void* operator new[](size_t size, std::align_val_t alignment)
{
	return AllocateAccountedAligned(size, alignment);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
	return TryAllocateAccounted(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
	return TryAllocateAccounted(size);
}

void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return TryAllocateAccountedAligned(size, alignment);
}

void* operator new[](size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
	return TryAllocateAccountedAligned(size, alignment);
}

void operator delete(void* ptr) noexcept
{
	Deallocate(ptr, 0);
}

void operator delete(void* ptr, size_t size) noexcept
{
	Deallocate(ptr, size);
}

void operator delete[](void* ptr) noexcept
{
	Deallocate(ptr, 0);
}

void operator delete[](void* ptr, size_t size) noexcept
{
	Deallocate(ptr, size);
}

void operator delete(void* ptr, std::align_val_t alignment) noexcept
{
	Deallocate(ptr, static_cast<size_t>(alignment));
}

void operator delete(void* ptr, size_t size, std::align_val_t) noexcept
{
	Deallocate(ptr, size);
}

void operator delete[](void* ptr, std::align_val_t alignment) noexcept
{
	Deallocate(ptr, static_cast<size_t>(alignment));
}

void operator delete[](void* ptr, size_t size, std::align_val_t) noexcept
{
	Deallocate(ptr, size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
	Deallocate(ptr, 0);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept
{
	Deallocate(ptr, 0);
}
