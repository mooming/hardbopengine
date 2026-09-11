// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Container/BoundedPriorityQueue.h"

#ifdef __UNIT_TEST__

#include <chrono>
#include <queue>
#include "Core/ScopedTime.h"


namespace hbe
{

	struct TestItem final
	{
		uint8_t priority;
		bool finished;

		TestItem() : priority(0), finished(false) {}
		TestItem(uint8_t p, bool f = false) : priority(p), finished(f) {}

		bool hasFinished() const { return finished; }

		bool operator<(const TestItem& other) const { return priority < other.priority; }
	};

	void BoundedPriorityQueueTest::prepare()
	{
		addTest("Push and Pop basic", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			queue.push(TestItem(10));
			queue.push(TestItem(5));
			queue.push(TestItem(15));

			auto item = queue.pop();
			if (!item.has_value())
			{
				ls << "Pop returned nullopt";
				return;
			}

			if (item->priority != 5)
			{
				ls << "Expected priority 5, got " << item->priority;
				return;
			}

			if (queue.Size() != 2)
			{
				ls << "Expected size 2, got " << queue.Size();
				return;
			}

			ls << "Pass";
		});

		addTest("Pop priority order", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem, 256> queue;

			queue.push(TestItem(200));
			queue.push(TestItem(50));
			queue.push(TestItem(100));
			queue.push(TestItem(10));

			auto item1 = queue.pop();
			if (!item1.has_value() || item1->priority != 10)
			{
				ls << "First pop should be priority 10";
				return;
			}

			auto item2 = queue.pop();
			if (!item2.has_value() || item2->priority != 50)
			{
				ls << "Second pop should be priority 50";
				return;
			}

			auto item3 = queue.pop();
			if (!item3.has_value() || item3->priority != 100)
			{
				ls << "Third pop should be priority 100";
				return;
			}

			auto item4 = queue.pop();
			if (!item4.has_value() || item4->priority != 200)
			{
				ls << "Fourth pop should be priority 200";
				return;
			}

			if (!queue.IsEmpty())
			{
				ls << "Queue should be empty";
				return;
			}

			ls << "Pass";
		});

		addTest("Pop from empty returns nullopt", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			auto item = queue.pop();
			if (item.has_value())
			{
				ls << "Expected nullopt from empty queue";
				return;
			}

			ls << "Pass";
		});

		addTest("Top returns highest priority without removing", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			queue.push(TestItem(100));
			queue.push(TestItem(50));
			queue.push(TestItem(75));

			auto top1 = queue.top();
			if (!top1.has_value() || top1->priority != 50)
			{
				ls << "First top should be 50";
				return;
			}

			auto top2 = queue.top();
			if (!top2.has_value() || top2->priority != 50)
			{
				ls << "Second top should also be 50";
				return;
			}

			if (queue.Size() != 3)
			{
				ls << "Size should still be 3, got " << queue.Size();
				return;
			}

			ls << "Pass";
		});

		addTest("PushRange", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			HVector<TestItem> items;
			items.push_back(TestItem(30));
			items.push_back(TestItem(10));
			items.push_back(TestItem(20));

			queue.pushRange(items);

			if (queue.Size() != 3)
			{
				ls << "Expected size 3, got " << queue.Size();
				return;
			}

			auto item = queue.pop();
			if (!item.has_value() || item->priority != 10)
			{
				ls << "First item should be priority 10";
				return;
			}

			ls << "Pass";
		});

		addTest("Remove finished items", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			queue.push(TestItem(10, true));   // finished
			queue.push(TestItem(20, false));
			queue.push(TestItem(15, true));   // finished
			queue.push(TestItem(25, false));

			if (queue.Size() != 4)
			{
				ls << "Expected size 4, got " << queue.Size();
				return;
			}

			auto removed = queue.Remove([](const TestItem& item)
			{
				return item.hasFinished();
			});

			if (removed != 2)
			{
				ls << "Expected 2 removed, got " << removed;
				return;
			}

			if (queue.Size() != 2)
			{
				ls << "Expected size 2 after remove, got " << queue.Size();
				return;
			}

			auto item = queue.pop();
			if (!item.has_value() || item->priority != 20)
			{
				ls << "First remaining should be priority 20";
				return;
			}

			item = queue.pop();
			if (!item.has_value() || item->priority != 25)
			{
				ls << "Second remaining should be priority 25";
				return;
			}

			ls << "Pass";
		});

		addTest("Clear", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			queue.push(TestItem(10));
			queue.push(TestItem(20));

			queue.clear();

			if (!queue.IsEmpty())
			{
				ls << "Queue should be empty after Clear";
				return;
			}

			if (queue.Size() != 0)
			{
				ls << "Size should be 0, got " << queue.Size();
				return;
			}

			ls << "Pass";
		});

		addTest("Move semantics", [](TLogOut& ls)
		{
			BoundedPriorityQueue<TestItem> queue;

			TestItem item(42);
			queue.push(std::move(item));

			auto popped = queue.pop();
			if (!popped.has_value() || popped->priority != 42)
			{
				ls << "Move semantics failed";
				return;
			}

			ls << "Pass";
		});

		addTest("Performance comparison with std::priority_queue", [this](TLogOut& ls)
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
						queue.push(TestItem(static_cast<uint8_t>(i % 256)));
					}

					while (!queue.IsEmpty())
					{
						(void)queue.pop();
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
