// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __UNIT_TEST__

#include "InlineMonotonicAllocator.h"

#include "AllocatorScope.h"
#include "HSTL/HVector.h"
#include "String/String.h"

namespace hbe
{

void InlineMonotonicAllocatorTest::prepare() noexcept
{
	using namespace std;
	using Allocator = InlineMonotonicAllocator<2048>;

	addTest("Vector Allocation", [this](auto& ls)
	{
		Allocator alloc("InlineMonotonicAllocator");

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
		Allocator alloc("InlineMonotonicAllocator");

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
		Allocator alloc("InlineMonotonicAllocator");
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
		Allocator alloc("InlineMonotonicAllocator");
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
