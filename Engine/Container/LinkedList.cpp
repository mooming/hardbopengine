// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "LinkedList.h"

#ifdef TEST_ENABLED

#include <list>

#include "Core/Debug.h"
#include "Core/ScopedTime.h"
#include "Memory/AllocatorScope.h"
#include "Memory/PoolAllocator.h"

namespace hbe
{
namespace
{
template <typename T>
class CountingAllocator final
{
public:
	using value_type = T;

	inline static int allocateCount = 0;
	inline static int deallocateCount = 0;

	T* allocate(std::size_t n) noexcept
	{
		allocateCount += static_cast<int>(n);

		return static_cast<T*>(::operator new(sizeof(T) * n));
	}

	void deallocate(T* ptr, std::size_t n) noexcept
	{
		deallocateCount += static_cast<int>(n);
		::operator delete(ptr);
	}
};
} // namespace

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

	AddTest("Clear leaves no tail behind", [this](auto& ls)
	{
		LinkedList<int> intList;
		intList.Add(7);
		intList.Clear();

		if (!intList.IsEmpty())
		{
			ls << "Clear left a list that still reports content." << lferr;

			return;
		}

		intList.Add(9);

		if (intList.Count(9) != 1 || intList.Count(7) != 0)
		{
			ls << "Add after Clear wrote to the wrong list: count(9) = " << static_cast<int>(intList.Count(9))
			   << ", count(7) = " << static_cast<int>(intList.Count(7)) << '.' << lferr;

			return;
		}

		ls << "Add after Clear lands in an empty list with no tail to walk." << lf;
	});

	AddTest("Move assignment releases the nodes it owned", [this](auto& ls)
	{
		using Alloc = CountingAllocator<LinkedListNode<int>>;
		using List = LinkedList<int, Alloc>;

		Alloc::allocateCount = 0;
		Alloc::deallocateCount = 0;

		List destination;
		destination.Add(1);

		const int allocations = Alloc::allocateCount;
		const int deallocations = Alloc::deallocateCount;

		List source;
		source.Add(2);

		destination = std::move(source);

		if (Alloc::deallocateCount != deallocations + 1)
		{
			ls << "Move assignment released " << Alloc::deallocateCount - deallocations
			   << " node(s); it owed exactly 1, so the destination's old node leaked." << lferr;

			return;
		}

		if (Alloc::allocateCount != allocations + 1)
		{
			ls << "Move assignment allocated " << Alloc::allocateCount - allocations << " node(s) instead of none."
			   << lferr;

			return;
		}

		int seen = 0;
		for (const int value : destination)
		{
			if (value != 2)
			{
				ls << "Move assignment left " << value << " in the destination instead of 2." << lferr;

				return;
			}

			++seen;
		}

		if (seen != 1 || !source.IsEmpty())
		{
			ls << "After the move the destination held " << seen << " node(s) and the source was "
			   << (source.IsEmpty() ? "empty" : "not empty") << '.' << lferr;

			return;
		}

		ls << "The displaced node is released exactly once." << lf;
	});
}
} // namespace hbe
#endif //TEST_ENABLED
