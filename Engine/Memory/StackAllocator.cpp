// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "StackAllocator.h"

#include "AllocatorScope.h"
#include "Config/EngineConfig.h"
#include "Core/Debug.h"
#include "MemoryManager.h"
#include "OSAL/Intrinsic.h"


namespace hbe
{

#if PROFILE_ENABLED
StackAllocator::StackAllocator(const char* name, SizeType inCapacity, const TSrcLoc& location) :
	id(InvalidAllocatorID), parentID(InvalidAllocatorID), capacity(inCapacity), cursor(0), buffer(nullptr),
	srcLocation(location)
#else // PROFILE_ENABLED
StackAllocator::StackAllocator(const char* name, SizeType inCapacity) :
	id(InvalidAllocatorID), capacity(inCapacity), cursor(0), buffer(nullptr)
#endif // PROFILE_ENABLED
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
		auto allocator = static_cast<StackAllocator*>(allocatorPtr);
		return allocator->allocate(n);
	};

	auto deallocFunc = [](void* allocatorPtr, void* ptr, size_t size)
	{
		auto allocator = static_cast<StackAllocator*>(allocatorPtr);
		allocator->Deallocate(ptr, size);
	};

	id = mmgr.registerAllocator(this, name, false, capacity, allocFunc, deallocFunc);
}

StackAllocator::~StackAllocator()
{
	auto& mmgr = MemoryManager::getInstance();
	mmgr.Deallocate(bufferPtr, capacity);

#if PROFILE_ENABLED
	mmgr.deregisterAllocator(getID(), srcLocation);
#else // PROFILE_ENABLED
	mmgr.deregisterAllocator(getID());
#endif // PROFILE_ENABLED
}

void* StackAllocator::allocate(const size_t requested)
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
		return mmgr.fallbackAllocate(getID(), parentID, requested);
	}

	auto ptr = reinterpret_cast<void*>(buffer + cursor);
	cursor += size;

#if PROFILE_ENABLED
	{
		auto& mmgr = MemoryManager::getInstance();
		mmgr.reportAllocation(id, ptr, size, size);
	}
#endif // PROFILE_ENABLED

	return ptr;
}

void StackAllocator::Deallocate(Pointer ptr, const SizeType requested) noexcept
{
	auto& mmgr = MemoryManager::getInstance();

	if (unlikely(!isMine(ptr)))
	{
		mmgr.Deallocate(parentID, ptr, requested);
		return;
	}

	SizeType size = requested;

	{
		constexpr auto AlignUnit = Config::DefaultAlign;
		const auto multiplier = (size + AlignUnit - 1) / AlignUnit;
		size = multiplier * AlignUnit;
	}

	Assert(cursor >= size);

	auto expected = buffer + cursor;
	auto provided = static_cast<Byte*>(ptr) + size;
	if (unlikely(expected != provided))
	{
		mmgr.logError([this, ptr, expected, provided](auto& lout)
		{
			lout << "StackAllocator[" << static_cast<int>(getID()) << "]: Pointer mismatched! ptr = " << ptr << ", "
				 << static_cast<void*>(expected) << " is expected. But " << static_cast<void*>(provided)
				 << " is provided.";
		});

		Assert(false);
		return;
	}

	cursor -= size;

#if PROFILE_ENABLED
	mmgr.reportDeallocation(id, ptr, size, size);
#endif // PROFILE_ENABLED
}

size_t StackAllocator::getAvailable() const
{
	Assert(capacity >= cursor);
	return capacity - cursor;
}

size_t StackAllocator::getUsage() const
{
	Assert(cursor < capacity);
	return cursor;
}

bool StackAllocator::isMine(Pointer ptr) const
{
	if (ptr < static_cast<void*>(buffer))
	{
		return false;
	}

	if (ptr >= static_cast<void*>(buffer + capacity))
	{
		return false;
	}

	return true;
}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "HSTL/HVector.h"
#include "ScopedAllocator.h"
#include "String/String.h"

namespace hbe
{

void StackAllocatorTest::prepare()
{
	using namespace std;
	using namespace hbe;

	addTest("Vector Allocation", [this](auto& ls)
	{
		StackAllocator stack("Test::StackAllocator", 1024 * 1024);

		{
			AllocatorScope scope(stack.getID());

			HVector<int> a;
			a.push_back(0);

#if PROFILE_ENABLED
			if (stack.getUsage() > 0)
			{
				ls << "Allocation Failed. Usage should not be zero, but " << stack.getUsage() << lferr;
			}
#else
			ls << "Profile Disabled" << lf;
#endif // PROFILE_ENABLED
		}

#if PROFILE_ENABLED
		if (stack.getUsage() != 0)
		{
			ls << "Deallocation Failed. Usage should be zero, but " << stack.getUsage() << lferr;
		}
#endif // PROFILE_ENABLED
	});

	addTest("Allocation (2)", [this](auto& ls)
	{
		StackAllocator stack("Test::StackAllocator::Allocation (2)", 1024 * 1024);

		{
			AllocatorScope scope(stack.getID());

			HVector<int> a;
			a.push_back(0);

			HVector<int> b;
			b.push_back(1);

#if PROFILE_ENABLED
			if (stack.getUsage() > 0)
			{
				ls << "Allocation Failed. Usage should not be zero, but " << stack.getUsage() << lferr;
			}
#else
			ls << "Profile Disabled" << lf;
#endif // PROFILE_ENABLED
		}

#if PROFILE_ENABLED
		if (stack.getUsage() != 0)
		{
			ls << "Deallocation Failed. Usage should be zero, but " << stack.getUsage() << lferr;
		}
#endif // PROFILE_ENABLED
	});

	addTest("Deallocation", [this](auto& ls)
	{
		StackAllocator stack("Test::StackAllocator::Deallocation", 1024 * 1024);
		AllocatorScope scope(stack.getID());

		{
			String a = "0";

			if (stack.getUsage() <= 0)
			{
				ls << "Allocation Failed. Usage should not be zero, but " << stack.getUsage() << lferr;
			}
		}

		if (stack.getUsage() != 0)
		{
			ls << "Deallocation Failed. Usage should be zero, but " << stack.getUsage() << lferr;
		}
	});

	addTest("Deallocation (2)", [this](auto& ls)
	{
		StackAllocator stack("Test::StackAllocator", 1024 * 1024);
		AllocatorScope scope(stack.getID());

		{
			String a = "0";
			String b = "1";

			if (stack.getUsage() <= 0)
			{
				ls << "Allocation Failed. Usage should not be zero, but " << stack.getUsage() << lferr;
			}
		}

		if (stack.getUsage() != 0)
		{
			ls << "Deallocation Failed. Usage should be zero, but " << stack.getUsage() << lferr;
		}
	});

	addTest("Nested Usage", [this](auto& ls)
	{
		using TAlloc = StackAllocator;
		int depthSeed = 0;

		ScopedAllocator<TAlloc> scope0("NestedStack0", 1024);

		const auto depth = depthSeed++;
		ls << "Neted Level " << depth << ", free size = " << scope0.getAllocator().getAvailable() << lf;

		{
			ScopedAllocator<TAlloc> scope1("NestedStack1", 512);

			auto ptr = New<long double>(0);

			const auto depth = depthSeed++;
			ls << "Neted Level " << depth << ", free size = " << scope1.getAllocator().getAvailable() << " / "
			   << scope0.getAllocator().getAvailable() << lf;

			{
				ScopedAllocator<TAlloc> scope2("NestedStack2", 256);

				auto ptr = New<long double>(0);

				const auto depth = depthSeed++;
				ls << "Neted Level " << depth << ", free size = " << scope2.getAllocator().getAvailable() << " / "
				   << scope1.getAllocator().getAvailable() << " / " << scope0.getAllocator().getAvailable() << lf;

				{
					ScopedAllocator<TAlloc> scope3("NestedStack3", 128);

					auto ptr = New<long double>(0);

					const auto depth = depthSeed++;
					ls << "Neted Level " << depth << ", free size = " << scope3.getAllocator().getAvailable() << " / "
					   << scope2.getAllocator().getAvailable() << " / " << scope1.getAllocator().getAvailable() << " / "
					   << scope0.getAllocator().getAvailable() << lf;

					{
						ScopedAllocator<TAlloc> scope4("NestedStack4", 64);

						auto ptr = New<long double>(0);

						const auto depth = depthSeed++;
						ls << "Neted Level " << depth << ", free size = " << scope4.getAllocator().getAvailable()
						   << " / " << scope3.getAllocator().getAvailable() << " / "
						   << scope2.getAllocator().getAvailable() << " / " << scope1.getAllocator().getAvailable()
						   << " / " << scope0.getAllocator().getAvailable() << lf;

						{
							ScopedAllocator<TAlloc> scope5("NestedStack05", 32);

							auto ptr = New<long double>(0);

							const auto depth = depthSeed++;
							ls << "Neted Level " << depth << ", free size = " << scope5.getAllocator().getAvailable()
							   << " / " << scope4.getAllocator().getAvailable() << " / "
							   << scope3.getAllocator().getAvailable() << " / " << scope2.getAllocator().getAvailable()
							   << " / " << scope1.getAllocator().getAvailable() << " / "
							   << scope0.getAllocator().getAvailable() << lf;

							Delete(ptr);
						}

						Delete(ptr);

						ls << "Neted Level " << depth << ", free size = " << scope4.getAllocator().getAvailable()
						   << " / " << scope3.getAllocator().getAvailable() << " / "
						   << scope2.getAllocator().getAvailable() << " / " << scope1.getAllocator().getAvailable()
						   << " / " << scope0.getAllocator().getAvailable() << lf;
					}

					Delete(ptr);

					ls << "Neted Level " << depth << ", free size = " << scope3.getAllocator().getAvailable() << " / "
					   << scope2.getAllocator().getAvailable() << " / " << scope1.getAllocator().getAvailable() << " / "
					   << scope0.getAllocator().getAvailable() << lf;
				}

				Delete(ptr);

				ls << "Neted Level " << depth << ", free size = " << scope2.getAllocator().getAvailable() << " / "
				   << scope1.getAllocator().getAvailable() << " / " << scope0.getAllocator().getAvailable() << lf;
			}

			Delete(ptr);

			ls << "Neted Level " << depth << ", free size = " << scope1.getAllocator().getAvailable() << " / "
			   << scope0.getAllocator().getAvailable() << lf;
		}

		ls << "Neted Level " << depth << ", free size = " << scope0.getAllocator().getAvailable() << lf;
	});
}

} // namespace hbe

#endif //__UNIT_TEST__
