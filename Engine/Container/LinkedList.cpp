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

void LinkedListTest::prepare()
{
	constexpr int CountBase = 1024;
	// One size in every configuration. It used to be CountBase * 2 under __DEBUG__, which meant
	// the suite a developer ran tested an eighth of what CI ran, and this was the only place
	// where __DEBUG__ changed *what* got tested rather than how loudly a failure reported.
	constexpr int COUNT = CountBase * 16;
	constexpr int COUNT2 = CountBase;

	addTest("Iteration on the empty list", [this](auto& ls)
	{
		LinkedList<int> intList;

		for (auto value : intList)
		{
			ls << "It iterates a loop even if the list is empty. value = " << value << lferr;

			break;
		}
	});

	addTest("Simple Construction & Destruction", [this](auto& ls)
	{
		const auto NodeSize = sizeof(LinkedList<int>::Node);

		PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
		AllocatorScope allocScope(alloc);

		{
			LinkedList<int> intList;

			for (int i = 0; i < COUNT; ++i)
			{
				intList.add(i);
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

	addTest("Growth and Iteration", [this](auto& ls)
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
				intList.add(i);
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

		ls << "Insert Time Compare : HE = " << time::toFloat(heTime) << ", STL = " << time::toFloat(stlTime) << lf;

		if (heTime > stlTime)
		{
			ls << "LinkedList is slower than the STL list" << std::endl
			   << "HE = " << time::toFloat(heTime) << ", STL = " << time::toFloat(stlTime) << lfwarn;
		}
	});

	addTest("Growth and Iteration", [this](auto& ls)
	{
		const auto NodeSize = sizeof(LinkedList<int>::Node);
		PoolAllocator alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
		AllocatorScope allocScope(alloc.getID());

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
				intList.add(i);
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

		ls << "Loop Time Compare : HE = " << time::toFloat(heTime) << ", STL = " << time::toFloat(stlTime) << lf;

		if (heTime > stlTime)
		{
			ls << "Lower Performance than STL list." << lfwarn;
		}
	});
}

} // namespace hbe
#endif //__UNIT_TEST__
