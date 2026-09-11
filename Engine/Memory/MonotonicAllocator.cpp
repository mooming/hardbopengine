// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MonotonicAllocator.h"

#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"
#include "Core/Debug.h"
#include "MemoryManager.h"


namespace hbe
{

MonotonicAllocator::MonotonicAllocator(const char* name, TSize inCapacity) :
	id(InvalidAllocatorID), cursor(0), capacity(inCapacity), buffer(nullptr)
{
	{
		constexpr auto AlignUnit = Config::DefaultAlign;
		const auto multiplier = (capacity + AlignUnit - 1) / AlignUnit;
		capacity = multiplier * AlignUnit;
	}

	auto& mmgr = MemoryManager::getInstance();
	parentID = hbe::MemoryManager::getCurrentAllocatorID();
	bufferPtr = mmgr.allocate(capacity);

	auto allocFunc = [](void* allocatorPtr, size_t n) -> void*
	{
		auto allocator = static_cast<MonotonicAllocator*>(allocatorPtr);
		return allocator->allocate(n);
	};

	auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t size)
	{
		auto allocator = static_cast<MonotonicAllocator*>(allocatorPtr);
		allocator->Deallocate(ptr, size);
	};

	id = mmgr.registerAllocator(this, name, false, capacity, allocFunc, deallocFunc);
}

MonotonicAllocator::~MonotonicAllocator()
{
	auto& mmgr = MemoryManager::getInstance();

#if PROFILE_ENABLED
	mmgr.reportDeallocation(id, bufferPtr, 0, cursor);
#endif // PROFILE_ENABLED

	mmgr.Deallocate(bufferPtr, capacity);
	mmgr.deregisterAllocator(getID());
}

void* MonotonicAllocator::allocate(const size_t requested)
{
	size_t size = requested;

	{
		constexpr auto AlignUnit = Config::DefaultAlign;
		const auto multiplier = (size + AlignUnit - 1) / AlignUnit;
		size = multiplier * AlignUnit;
	}

	const auto freeSize = getAvailable();
	if (unlikely(size > freeSize))
	{
		auto& mmgr = MemoryManager::getInstance();
		mmgr.logWarning([size, freeSize](auto& ls)
		{ ls << "The requested size " << size << " is exceeding its limit, " << freeSize << '.'; });

		auto ptr = mmgr.allocate(parentID, requested);

		return ptr;
	}

	auto ptr = reinterpret_cast<void*>(buffer + cursor);
	cursor += size;

#if PROFILE_ENABLED
	{
		auto& mmgr = MemoryManager::getInstance();
		mmgr.reportAllocation(id, ptr, requested, size);
	}
#endif // PROFILE_ENABLED

	return ptr;
}

void MonotonicAllocator::Deallocate(Pointer ptr, TSize requested) noexcept
{
	auto& mmgr = MemoryManager::getInstance();

	if (unlikely(!isMine(ptr)))
	{
		mmgr.Deallocate(parentID, ptr, requested);
		return;
	}

#if MEMORY_LOGGING_ENABLED
	mmgr.log(ELogLevel::Verbose, [this, &mmgr, ptr, requested](auto& lout)
	{
		lout << mmgr.getAllocatorName(id) << '[' << static_cast<int>(getID())
			 << "] Deallocate call shall be ignored. ptr = " << static_cast<void*>(ptr)
			 << ", requested size = " << requested;
	});
#endif // MEMORY_LOGGING_ENABLED

#if PROFILE_ENABLED
	mmgr.reportDeallocation(id, ptr, requested, 0);
#endif // PROFILE_ENABLED
}

size_t MonotonicAllocator::getAvailable() const
{
	Assert(capacity >= cursor);
	return capacity - cursor;
}

size_t MonotonicAllocator::getUsage() const
{
	Assert(cursor < capacity);
	return cursor;
}

	bool MonotonicAllocator::isMine(TPointer ptr) const
	{
		auto bytePtr = static_cast<uint8_t*>(ptr);
		if (bytePtr < buffer)
		{
			return false;
		}

		if (bytePtr >= (buffer + capacity))
		{
			return false;
		}

		return true;
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "HSTL/HVector.h"
#include "String/String.h"

namespace hbe
{

void MonotonicAllocatorTest::prepare()
{
	using namespace std;
	using namespace hbe;

	addTest("Vector Allocation", [this](auto& ls)
	{
		MonotonicAllocator alloc("Test::MonotonicAllocator", 1024 * 1024);

		{
			AllocatorScope scope(alloc.getID());

			HVector<int> a;
			a.push_back(0);
		}

		if (alloc.getUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.getUsage() << lferr;
		}
	});

	addTest("Allocation (2)", [this](auto& ls)
	{
		MonotonicAllocator alloc("Test::MonotonicAllocator", 1024 * 1024);

		{
			AllocatorScope scope(alloc.getID());

			HVector<int> a;
			a.push_back(0);

			HVector<int> b;
			b.push_back(1);
		}

		if (alloc.getUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.getUsage() << lferr;
		}
	});

	addTest("Deallocation", [this](auto& ls)
	{
		MonotonicAllocator alloc("Test::MonotonicAllocator", 1024);
		AllocatorScope scope(alloc.getID());

		{
			String a = "0";
		}

		if (alloc.getUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.getUsage() << lferr;
		}
	});

	addTest("Deallocation (2)", [this](auto& ls)
	{
		MonotonicAllocator alloc("Test::MonotonicAllocator", 1024);
		AllocatorScope scope(alloc.getID());

		{
			String a = "0";
			String b = "1";
		}

		if (alloc.getUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.getUsage() << lferr;
		}
	});
}

} // namespace hbe

#endif //__UNIT_TEST__
