// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef TEST_ENABLED

#include "InlineMonotonicAllocator.h"

#include "AllocatorScope.h"
#include "HSTL/HVector.h"
#include "String/String.h"

namespace hbe
{
void InlineMonotonicAllocatorTest::Prepare() noexcept
{
	using namespace std;
	using Allocator = InlineMonotonicAllocator<2048>;

	AddTest("Vector Allocation", [this](auto& ls)
	{
		Allocator alloc("InlineMonotonicAllocator");
		{
			AllocatorScope scope(alloc.GetID());

			HVector<int> a;
			a.push_back(0);
		}

		if (alloc.GetUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
		}
	});

	AddTest("Allocation (2)", [this](auto& ls)
	{
		Allocator alloc("InlineMonotonicAllocator");
		{
			AllocatorScope scope(alloc.GetID());

			HVector<int> a;
			a.push_back(0);

			HVector<int> b;
			b.push_back(1);
		}

		if (alloc.GetUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
		}
	});

	AddTest("Deallocation", [this](auto& ls)
	{
		Allocator alloc("InlineMonotonicAllocator");
		AllocatorScope scope(alloc.GetID());
		{
			String a = "0";
		}

		if (alloc.GetUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
		}
	});

	AddTest("Deallocation (2)", [this](auto& ls)
	{
		Allocator alloc("InlineMonotonicAllocator");
		AllocatorScope scope(alloc.GetID());
		{
			String a = "0";
			String b = "1";
		}

		if (alloc.GetUsage() == 0)
		{
			ls << "Monotonic Allocator doesn't provide deallocation."
			   << " Usage should not be zero, but " << alloc.GetUsage() << lferr;
		}
	});

		AddTest("Destroy In Own Scope", [this](auto& ls)
	{
		static void* claimedPtr = nullptr;
		static void* releasedPtr = nullptr;
		claimedPtr = nullptr;
		releasedPtr = nullptr;

		auto claim = [](void*, const size_t n) -> void*
		{
			claimedPtr = ::operator new(n);

			return claimedPtr;
		};

		auto release = [](void*, void* ptr, const size_t n)
		{
			releasedPtr = ptr;
			::operator delete(ptr, n);
		};

		auto& mmgr = MemoryManager::GetInstance();
		constexpr size_t capacity = 128 * 1024;
		const auto parentID =
				mmgr.RegisterAllocator(nullptr, "Test::InlineMonotonicParent", false, capacity, claim, release);
		{
			AllocatorScope parentScope(parentID);

			auto alloc = new Allocator("InlineMonotonicAllocator");
			AllocatorScope scope(alloc->GetID());
			delete alloc;
		}

		mmgr.DeregisterAllocator(parentID);

		if (claimedPtr != nullptr || releasedPtr != nullptr)
		{
			ls << "the class owns no heap buffer, but its construction claimed " << claimedPtr
			   << " and its destruction released " << releasedPtr << " through the parent." << lferr;

			return;
		}

		ls << "No buffer was claimed and none was released, which is the whole contract of an inline buffer." << lf;
	});
}
} // namespace hbe

#endif // TEST_ENABLED
