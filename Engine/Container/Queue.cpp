// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Queue.h"

#ifdef __UNIT_TEST__
#include <queue>
#include "Core/ScopedTime.h"

namespace hbe
{

	void QueueTest::Prepare()
	{
		AddTest("Default Construction", [](auto&) { Queue<int> q; });

		AddTest("Push and Pop", [this](auto& ls)
		{
			Queue<int> q;
			q.Push(10);
			q.Push(20);
			q.Push(30);

			if (q.Size() != 3)
			{
				ls << "Expected size 3, got " << q.Size() << lferr;
				return;
			}

			if (q.Front() != 10 || q.Back() != 30)
			{
				ls << "Front/Back mismatch" << lferr;
				return;
			}

			q.Pop();
			if (q.Front() != 20 || q.Size() != 2)
			{
				ls << "Pop failed" << lferr;
				return;
			}

			q.Pop();
			q.Pop();
			if (!q.IsEmpty())
			{
				ls << "Queue should be empty" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("FIFO Ordering", [this](auto& ls)
		{
			Queue<int> q;

			for (int i = 0; i < 100; ++i)
			{
				q.Push(i);
			}

			for (int i = 0; i < 100; ++i)
			{
				if (q.Front() != i)
				{
					ls << "Expected " << i << ", got " << q.Front() << lferr;
					return;
				}

				q.Pop();
			}

			ls << "Pass";
		});

		AddTest("Emplace", [this](auto& ls)
		{
			Queue<std::pair<int, int>> q;
			q.Emplace(1, 2);
			q.Emplace(3, 4);

			if (q.Size() != 2 || q.Front().first != 1)
			{
				ls << "Emplace failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Move Semantics", [this](auto& ls)
		{
			Queue<int> q1;
			q1.Push(1);
			q1.Push(2);

			Queue<int> q2(std::move(q1));
			if (q2.Size() != 2 || q2.Front() != 1)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			Queue<int> q3;
			q3 = std::move(q2);
			if (q3.Size() != 2 || q3.Front() != 1)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Clear", [this](auto& ls)
		{
			Queue<int> q;
			q.Push(1);
			q.Push(2);
			q.Clear();

			if (!q.IsEmpty())
			{
				ls << "Queue should be empty after Clear" << lferr;
				return;
			}

			q.Push(10);
			if (q.Size() != 1 || q.Front() != 10)
			{
				ls << "Reuse after Clear failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Performance vs std::queue", [this](auto& ls)
		{
			constexpr int NumItems = 100000;
			constexpr int NumIterations = 20;

			time::TDuration hePushTime;
			time::TDuration stlPushTime;
			time::TDuration hePopTime;
			time::TDuration stlPopTime;

			{
				time::ScopedTime measure(hePushTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					Queue<int> q;
					for (int i = 0; i < NumItems; ++i)
					{
						q.Push(i);
					}
				}
			}

			{
				time::ScopedTime measure(stlPushTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					std::queue<int> q;
					for (int i = 0; i < NumItems; ++i)
					{
						q.push(i);
					}
				}
			}

			{
				Queue<int> q;
				for (int i = 0; i < NumItems * NumIterations; ++i)
				{
					q.Push(i);
				}

				time::ScopedTime measure(hePopTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					for (int i = 0; i < NumItems; ++i)
					{
						q.Pop();
					}
				}
			}

			{
				std::queue<int> q;
				for (int i = 0; i < NumItems * NumIterations; ++i)
				{
					q.push(i);
				}

				time::ScopedTime measure(stlPopTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					for (int i = 0; i < NumItems; ++i)
					{
						q.pop();
					}
				}
			}

			ls << "Queue push: " << hePushTime.count() << " us" << lf;
			ls << "std::queue push: " << stlPushTime.count() << " us" << lf;
			ls << "Queue pop: " << hePopTime.count() << " us" << lf;
			ls << "std::queue pop: " << stlPopTime.count() << " us" << lf;

			if (hePushTime > stlPushTime * 2)
			{
				ls << "Queue push is slower than 2x std::queue" << lfwarn;
			}

			ls << "Pass";
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
