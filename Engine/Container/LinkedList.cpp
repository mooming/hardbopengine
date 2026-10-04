// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "LinkedList.h"

#ifdef __UNIT_TEST__

#include <list>

#include "Core/Debug.h"
#include "Core/ScopedTime.h"
#include "Memory/AllocatorScope.h"
#include "Memory/PoolAllocator.h"

namespace hbe
{
void LinkedListTest::Prepare()
{
	constexpr int CountBase = 1024;
	constexpr int COUNT = CountBase * 16;
	constexpr int COUNT2 = CountBase;

	AddTest("Iteration on the empty list", [this](auto& ls)
	{
		LinkedList<int> intList;

		for (auto value : intList)
		{
			ls << "It iterates a loop even if the list is empty. value = " << value << lferr;

			break;
		}
	});

	AddTest("Simple Construction & Destruction", [this](auto& ls)
	{
		const auto NodeSize = sizeof(LinkedList<int>::Node);

		PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
		AllocatorScope allocScope(alloc);
		{
			LinkedList<int> intList;

			for (int i = 0; i < COUNT; ++i)
			{
				intList.Add(i);
			}

			ls << "A list is constructed." << lf;

			int i = 0;
			for (auto value : intList)
			{
				if (value != i)
				{
					ls << "Value Mismatched : value = " << value << ", expected " << i << '.' << lferr;

					return;
				}

				++i;
			}
		}

		ls << "The list is destructed." << lf;
	});

	AddTest("Growth and Iteration", [this](auto& ls)
	{
		const auto NodeSize = sizeof(LinkedList<int>::Node);

		PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
		AllocatorScope allocScope(alloc);

		time::TDuration heTime;
		time::TDuration stlTime;
		{
			time::ScopedTime measure(heTime);

			LinkedList<int> intList;
			for (int i = 0; i < COUNT; ++i)
			{
				intList.Add(i);
			}

			int i = 0;
			for (auto value : intList)
			{
				if (value != i)
				{
					ls << "Value Mismatched : value = " << value << ", expected " << i << '.' << lferr;

					return;
				}

				++i;
			}
		}
		{
			time::ScopedTime measure(stlTime);

			std::list<int> intList;
			for (int i = 0; i < COUNT; ++i)
			{
				intList.push_back(i);
			}

			int i = 0;
			for (auto value : intList)
			{
				if (value != i)
				{
					ls << "Value Mismatched : value = " << value << ", expected " << i << "." << lferr;

					return;
				}

				++i;
			}
		}

		ls << "Insert Time Compare : HE = " << time::ToFloat(heTime) << ", STL = " << time::ToFloat(stlTime) << lf;

		if (heTime > stlTime)
		{
			ls << "LinkedList is slower than the STL list" << std::endl
			   << "HE = " << time::ToFloat(heTime) << ", STL = " << time::ToFloat(stlTime) << lfwarn;
		}
	});

	AddTest("Growth and Iteration", [this](auto& ls)
	{
		const auto NodeSize = sizeof(LinkedList<int>::Node);
		PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
		AllocatorScope allocScope(alloc.GetID());

		time::TDuration heTime;
		time::TDuration stlTime;

		long long stlValue = 0;
		long long heValue = 0;
		{
			std::list<int> intList;
			for (int i = 0; i < COUNT; ++i)
			{
				intList.push_back(i);
			}
			{
				time::ScopedTime measure(stlTime);
				for (int i = 0; i < COUNT2; ++i)
				{
					for (auto value : intList)
					{
						stlValue += value;
					}
				}
			}
		}
		{
			LinkedList<int> intList;
			for (int i = 0; i < COUNT; ++i)
			{
				intList.Add(i);
			}
			{
				time::ScopedTime measure(heTime);
				for (int i = 0; i < COUNT2; ++i)
				{
					for (auto value : intList)
					{
						heValue += value;
					}
				}
			}
		}

		if (heValue != stlValue)
		{
			ls << "Result Mismatched : HE = " << heValue << ", STL = " << stlValue << lferr;

			return;
		}

		ls << "Loop Time Compare : HE = " << time::ToFloat(heTime) << ", STL = " << time::ToFloat(stlTime) << lf;

		if (heTime > stlTime)
		{
			ls << "Lower Performance than STL list." << lfwarn;
		}
	});

	AddTest("Element-Reference Mutation", [this](auto& ls)
	{
		const auto NodeSize = sizeof(LinkedList<int>::Node);

		PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT2 + 10);
		AllocatorScope allocScope(alloc);

		LinkedList<int> intList;
		for (int i = 0; i < 8; ++i)
		{
			intList.Add(i * 10);
		}

		int* const middle = intList.Find(30);
		if (middle == nullptr)
		{
			ls << "Find(30) could not locate the element to mutate around." << lferr;

			return;
		}

		if (intList.AddNext(*middle, 35) != 35)
		{
			ls << "AddNext did not return the value it inserted." << lferr;

			return;
		}

		int* const inserted = intList.Find(35);
		if (inserted == nullptr)
		{
			ls << "AddNext did not link the new element into the list." << lferr;

			return;
		}

		if (intList.AddPrevious(*inserted, 25) != 25)
		{
			ls << "AddPrevious did not return the value it inserted." << lferr;

			return;
		}

		int* const removable = intList.Find(25);
		if (removable == nullptr)
		{
			ls << "AddPrevious did not link the new element into the list." << lferr;

			return;
		}

		intList.Remove(*removable);

		if (intList.Find(25) != nullptr)
		{
			ls << "Remove left the element reachable after removal." << lferr;

			return;
		}

		int count = 0;
		int previous = -10;
		for (const int value : intList)
		{
			if (value <= previous)
			{
				ls << "Order Broken : " << value << " follows " << previous << '.' << lferr;

				return;
			}

			previous = value;
			++count;
		}

		if (count != 9)
		{
			ls << "Size Mismatched : count = " << count << ", expected 9." << lferr;

			return;
		}

		ls << "Remove, AddNext and AddPrevious work on list-owned references." << lf;
	});
}
} // namespace hbe
#endif //__UNIT_TEST__
