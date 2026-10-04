// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Container/BoundedPriorityQueue.h"

#ifdef __UNIT_TEST__

#include <chrono>
#include <queue>

#include "Core/ScopedTime.h"
#include "HSTL/HVector.h"

namespace hbe
{
struct TestItem final
{
	uint8_t priority;
	bool finished;
	uint8_t tag;

	TestItem()
		: priority(0)
		, finished(false)
		, tag(0)
	{
	}

	TestItem(uint8_t p, bool f = false, uint8_t t = 0)
		: priority(p)
		, finished(f)
		, tag(t)
	{
	}

	bool HasFinished() const
	{
		return finished;
	}

	bool operator<(const TestItem& other) const
	{
		return priority < other.priority;
	}
};

void BoundedPriorityQueueTest::Prepare()
{
	AddTest("Push and Pop basic", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		queue.Push(TestItem(10));
		queue.Push(TestItem(5));
		queue.Push(TestItem(15));

		auto item = queue.Pop();
		if (!item.has_value())
		{
			ls << "Pop returned nullopt" << lferr;

			return;
		}

		if (item->priority != 15)
		{
			ls << "Expected priority 15 (highest = most urgent), got " << item->priority << lferr;

			return;
		}

		if (queue.Size() != 2)
		{
			ls << "Expected size 2, got " << queue.Size() << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Pop priority order", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem, 256> queue;

		queue.Push(TestItem(200));
		queue.Push(TestItem(50));
		queue.Push(TestItem(100));
		queue.Push(TestItem(10));

		auto item1 = queue.Pop();
		if (!item1.has_value() || item1->priority != 200)
		{
			ls << "First pop should be priority 200 (highest = most urgent)" << lferr;

			return;
		}

		auto item2 = queue.Pop();
		if (!item2.has_value() || item2->priority != 100)
		{
			ls << "Second pop should be priority 100" << lferr;

			return;
		}

		auto item3 = queue.Pop();
		if (!item3.has_value() || item3->priority != 50)
		{
			ls << "Third pop should be priority 50" << lferr;

			return;
		}

		auto item4 = queue.Pop();
		if (!item4.has_value() || item4->priority != 10)
		{
			ls << "Fourth pop should be priority 10" << lferr;

			return;
		}

		if (!queue.IsEmpty())
		{
			ls << "Queue should be empty" << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Equal priorities drain oldest first", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		queue.Push(TestItem(77, false, 1));
		queue.Push(TestItem(77, false, 2));
		queue.Push(TestItem(77, false, 3));

		for (uint8_t expectedTag = 1; expectedTag <= 3; ++expectedTag)
		{
			auto item = queue.Pop();
			if (!item.has_value())
			{
				ls << "Pop returned nullopt at tag " << expectedTag << lferr;

				return;
			}

			if (item->tag != expectedTag)
			{
				ls << "Equal priorities drained tag " << static_cast<int>(item->tag) << " before tag "
				   << static_cast<int>(expectedTag) << " - newest is winning the tie" << lferr;

				return;
			}
		}

		ls << "Pass";
	});

	AddTest("Bucket is released when a priority empties", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem, 256, 64> queue;

		for (uint8_t tag = 0; tag < 5; ++tag)
		{
			queue.Push(TestItem(200, false, tag));
		}

		for (uint8_t tag = 0; tag < 5; ++tag)
		{
			auto drained = queue.Pop();
			if (!drained.has_value() || drained->tag != tag)
			{
				ls << "Draining the priority dropped or reordered tag " << static_cast<int>(tag) << lferr;

				return;
			}
		}

		if (!queue.IsEmpty() || queue.Size() != 0)
		{
			ls << "Queue reports " << queue.Size() << " items after draining everything" << lferr;

			return;
		}

		// Pushing again must re-create the bucket rather than read a released one.
		queue.Push(TestItem(9, false, 42));
		auto item = queue.Pop();
		if (!item.has_value() || item->tag != 42)
		{
			ls << "A re-created bucket lost its item" << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Pop from empty returns nullopt", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		auto item = queue.Pop();
		if (item.has_value())
		{
			ls << "Expected nullopt from empty queue" << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Top returns highest priority without removing", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		queue.Push(TestItem(100));
		queue.Push(TestItem(50));
		queue.Push(TestItem(75));

		auto top1 = queue.Top();
		if (!top1.has_value() || top1->priority != 100)
		{
			ls << "First top should be 100" << lferr;

			return;
		}

		auto top2 = queue.Top();
		if (!top2.has_value() || top2->priority != 100)
		{
			ls << "Second top should also be 100" << lferr;

			return;
		}

		if (queue.Size() != 3)
		{
			ls << "Size should still be 3, got " << queue.Size() << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("PushRange", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		HVector<TestItem> items;
		items.push_back(TestItem(30));
		items.push_back(TestItem(10));
		items.push_back(TestItem(20));

		queue.PushRange(items);

		if (queue.Size() != 3)
		{
			ls << "Expected size 3, got " << queue.Size() << lferr;

			return;
		}

		auto item = queue.Pop();
		if (!item.has_value() || item->priority != 30)
		{
			ls << "First item should be priority 30 (highest = most urgent)" << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Remove finished items", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		queue.Push(TestItem(10, true)); // finished
		queue.Push(TestItem(20, false));
		queue.Push(TestItem(15, true)); // finished
		queue.Push(TestItem(25, false));

		if (queue.Size() != 4)
		{
			ls << "Expected size 4, got " << queue.Size() << lferr;

			return;
		}

		auto removed = queue.Remove([](const TestItem& item) { return item.HasFinished(); });

		if (removed != 2)
		{
			ls << "Expected 2 removed, got " << removed << lferr;

			return;
		}

		if (queue.Size() != 2)
		{
			ls << "Expected size 2 after remove, got " << queue.Size() << lferr;

			return;
		}

		auto item = queue.Pop();
		if (!item.has_value() || item->priority != 25)
		{
			ls << "First remaining should be priority 25 (highest = most urgent)" << lferr;

			return;
		}

		item = queue.Pop();
		if (!item.has_value() || item->priority != 20)
		{
			ls << "Second remaining should be priority 20" << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Clear", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		queue.Push(TestItem(10));
		queue.Push(TestItem(20));

		queue.Clear();

		if (!queue.IsEmpty())
		{
			ls << "Queue should be empty after Clear" << lferr;

			return;
		}

		if (queue.Size() != 0)
		{
			ls << "Size should be 0, got " << queue.Size() << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Move semantics", [this](TLogOut& ls)
	{
		BoundedPriorityQueue<TestItem> queue;

		TestItem item(42);
		queue.Push(std::move(item));

		auto popped = queue.Pop();
		if (!popped.has_value() || popped->priority != 42)
		{
			ls << "Move semantics failed" << lferr;

			return;
		}

		ls << "Pass";
	});

	AddTest("Performance comparison with std::priority_queue", [this](TLogOut& ls)
	{
		constexpr int NumItems = 10000;
		constexpr int NumIterations = 100;

		time::TDuration engineTime;
		time::TDuration stlTime;

		// Test BoundedPriorityQueue
		{
			time::ScopedTime measure(engineTime);
			BoundedPriorityQueue<TestItem> queue;

			for (int iter = 0; iter < NumIterations; ++iter)
			{
				for (int i = 0; i < NumItems; ++i)
				{
					queue.Push(TestItem(static_cast<uint8_t>(i % 256)));
				}
				while (!queue.IsEmpty())
				{
					(void) queue.Pop();
				}
			}
		}

		// Test std::priority_queue
		{
			time::ScopedTime measure(stlTime);
			std::priority_queue<TestItem> queue;

			for (int iter = 0; iter < NumIterations; ++iter)
			{
				for (int i = 0; i < NumItems; ++i)
				{
					queue.push(TestItem(static_cast<uint8_t>(i % 256)));
				}
				while (!queue.empty())
				{
					queue.pop();
				}
			}
		}

		ls << "BoundedPriorityQueue: " << engineTime.count() << " us" << lf;
		ls << "std::priority_queue: " << stlTime.count() << " us" << lf;

		if (engineTime > stlTime)
		{
			ls << "Lower Performance (" << engineTime.count() << ") than STL(" << stlTime.count() << ")." << lfwarn;
		}

		ls << "Pass";
	});
}
} // namespace hbe

#endif // __UNIT_TEST__
