// Created by mooming.go@gmail.com

#include "DefaultAllocator.h"

#ifdef __UNIT_TEST__
#include <vector>
#include "AllocatorScope.h"
#include "PoolAllocator.h"

void hbe::BaseAllocatorTest::Prepare()
{
	AddTest("Basic Usage", [this](auto& ls)
	{
		PoolAllocator alloc("BaseAllocTestAlloc", 1024, 1024);

		{
			AllocatorScope allocScope(alloc);
			std::vector<int, DefaultAllocator<int>> v;

			if (alloc.GetUsage() != 0)
			{
				ls << "Invalid allocator usage = " << alloc.GetUsage() << ", 0 is expected." << lferr;
			}

			v.reserve(1);

			if (alloc.GetUsage() == 0)
			{
				ls << "Invalid allocator usage = " << alloc.GetUsage() << ", non-zero value is expected." << lferr;
			}
		}
	});

	AddTest("Multiple Allocations", [](auto&)
	{
		for (int j = 0; j < 100; ++j)
		{
			std::vector<int, DefaultAllocator<int>> v;
			for (int i = 0; i < 2048; ++i)
			{
				v.push_back(i);
			}
		}
	});

	AddTest("Nested Allocations", [this](auto& ls)
	{
		{
			PoolAllocator alloc("BaseAllocTestAlloc", 1024, 1024);
			ls << "First Pool Allocator Buffer =  " << static_cast<void*>(alloc.GetBuffer()) << ", Allocated by "
			   << MemoryManager::GetCurrentAllocatorID() << lf;

			AllocatorScope allocScope(alloc);
			std::vector<int, DefaultAllocator<int>> v;

			if (alloc.GetUsage() != 0)
			{
				ls << "Invalid allocator usage = " << alloc.GetUsage() << ", 0 is expected." << lferr;
			}

			{
				PoolAllocator alloc2("BaseAllodTestAlloc2", 1024, 1024);
				ls << "Second Pool Allocator Buffer =  " << static_cast<void*>(alloc.GetBuffer()) << ", Allocated by "
				   << MemoryManager::GetCurrentAllocatorID() << lf;
				AllocatorScope allocScope2(alloc2);

				v.reserve(1);

				if (alloc2.GetUsage() != 0)
				{
					ls << "Invalid allocator 2 usage = " << alloc.GetUsage() << ", 0 is expected." << lferr;
				}

				if (alloc.GetUsage() == 0)
				{
					ls << "Invalid allocator usage = " << alloc.GetUsage() << ", non-zero value is expected." << lferr;
				}

				v.clear();
				v.shrink_to_fit();

				if (alloc2.GetUsage() != 0)
				{
					ls << "Invalid allocator 2 usage = " << alloc.GetUsage() << ", 0 is expected." << lferr;
				}

				if (alloc.GetUsage() != 0)
				{
					ls << "Invalid allocator usage = " << alloc.GetUsage() << ", 0 value is expected." << lferr;
				}

				v.reserve(100);

				if (alloc2.GetUsage() != 0)
				{
					ls << "Invalid allocator 2 usage = " << alloc.GetUsage() << ", 0 is expected." << lferr;
				}

				if (alloc.GetUsage() == 0)
				{
					ls << "Invalid allocator usage = " << alloc.GetUsage() << ", non-zero value is expected." << lferr;
				}
			}
		}
	});
}

#endif // __UNIT_TEST__
