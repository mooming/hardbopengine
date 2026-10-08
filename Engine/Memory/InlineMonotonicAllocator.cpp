// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __TEST__

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
		const auto allocBytesBefore = MemoryManager::GetGlobalAllocationBytes();
		const auto freeBytesBefore = MemoryManager::GetGlobalFreeBytes();
		{
			auto alloc = new Allocator("InlineMonotonicAllocator");
			AllocatorScope scope(alloc->GetID());
			delete alloc;
		}

		const auto allocBytes = MemoryManager::GetGlobalAllocationBytes() - allocBytesBefore;
		const auto freedBytes = MemoryManager::GetGlobalFreeBytes() - freeBytesBefore;

		if (freedBytes < allocBytes)
		{
			ls << "Destroying InlineMonotonicAllocator while its own AllocatorScope is open retained "
			   << (allocBytes - freedBytes) << " bytes; its destructor owns no buffer to release and shall start "
			   << "releasing one." << lferr;
		}
	});
}
} // namespace hbe

#endif //__TEST__
