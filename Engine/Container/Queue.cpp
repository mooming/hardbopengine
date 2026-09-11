// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Queue.h"

#ifdef __UNIT_TEST__
#include <queue>
#include "Core/ScopedTime.h"


namespace hbe
{

	void QueueTest::prepare()
	{
		addTest("Default Construction", [](auto&) { Queue<int> q; });

		addTest("Push and Pop", [this](auto& ls)
		{
			Queue<int> q;
			q.push(10);
			q.push(20);
			q.push(30);

			if (q.Size() != 3)
			{
				ls << "Expected size 3, got " << q.Size() << lferr;
				return;
			}

			if (q.front() != 10 || q.back() != 30)
			{
				ls << "Front/Back mismatch" << lferr;
				return;
			}

			q.pop();
			if (q.front() != 20 || q.Size() != 2)
			{
				ls << "Pop failed" << lferr;
				return;
			}

			q.pop();
			q.pop();
			if (!q.IsEmpty())
			{
				ls << "Queue should be empty" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("FIFO Ordering", [this](auto& ls)
		{
			Queue<int> q;

			for (int i = 0; i < 100; ++i)
			{
				q.push(i);
			}

			for (int i = 0; i < 100; ++i)
			{
				if (q.front() != i)
				{
					ls << "Expected " << i << ", got " << q.front() << lferr;
					return;
				}

				q.pop();
			}

			ls << "Pass";
		});

		addTest("Emplace", [this](auto& ls)
		{
			Queue<std::pair<int, int>> q;
			q.emplace(1, 2);
			q.emplace(3, 4);

			if (q.Size() != 2 || q.front().first != 1)
			{
				ls << "Emplace failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Move Semantics", [this](auto& ls)
		{
			Queue<int> q1;
			q1.push(1);
			q1.push(2);

			Queue<int> q2(std::move(q1));
			if (q2.Size() != 2 || q2.front() != 1)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			Queue<int> q3;
			q3 = std::move(q2);
			if (q3.Size() != 2 || q3.front() != 1)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Clear", [this](auto& ls)
		{
			Queue<int> q;
			q.push(1);
			q.push(2);
			q.clear();

			if (!q.IsEmpty())
			{
				ls << "Queue should be empty after Clear" << lferr;
				return;
			}

			q.push(10);
			if (q.Size() != 1 || q.front() != 10)
			{
				ls << "Reuse after Clear failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Performance vs std::queue", [this](auto& ls)
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
						q.push(i);
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
					q.push(i);
				}

				time::ScopedTime measure(hePopTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					for (int i = 0; i < NumItems; ++i)
					{
						q.pop();
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
