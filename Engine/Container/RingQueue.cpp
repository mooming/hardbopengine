// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RingQueue.h"

#ifdef __UNIT_TEST__
#include <queue>
#include "Core/ScopedTime.h"


namespace hbe
{

	void RingQueueTest::prepare()
	{
		addTest("Construction", [this](auto& ls)
		{
			RingQueue<int> q(8);

			if (q.Capacity() != 8 || !q.IsEmpty())
			{
				ls << "Construction failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Push and Pop", [this](auto& ls)
		{
			RingQueue<int> q(4);
			q.push(10);
			q.push(20);
			q.push(30);

			if (q.Size() != 3 || q.front() != 10 || q.back() != 30)
			{
				ls << "Push failed" << lferr;
				return;
			}

			auto v1 = q.pop();
			if (v1 != 10)
			{
				ls << "Pop should return 10, got " << v1 << lferr;
				return;
			}

			if (q.Size() != 2)
			{
				ls << "Expected size 2 after pop" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Wrap Around", [this](auto& ls)
		{
			RingQueue<int> q(4);
			q.push(1);
			q.push(2);
			q.push(3);
			q.pop();
			q.pop();
			q.push(4);
			q.push(5);

			if (q.Size() != 3 || q[0] != 3 || q[1] != 4 || q[2] != 5)
			{
				ls << "Expected 3,4,5 got " << q[0] << "," << q[1] << "," << q[2] << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Full Detection", [this](auto& ls)
		{
			RingQueue<int> q(4);  // Power of 2 capacity
			q.push(1);
			q.push(2);
			q.push(3);
			q.push(4);

			if (!q.isFull())
			{
				ls << "Queue should be full" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Emplace", [this](auto& ls)
		{
			RingQueue<std::pair<int, int>> q(4);
			q.emplace(10, 20);
			q.emplace(30, 40);

			if (q.Size() != 2 || q[0].first != 10)
			{
				ls << "Emplace failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Move Semantics", [this](auto& ls)
		{
			RingQueue<int> q1(4);
			q1.push(1);
			q1.push(2);

			RingQueue<int> q2(std::move(q1));
			if (q2.Size() != 2 || q2[0] != 1)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			RingQueue<int> q3(1);
			q3 = std::move(q2);
			if (q3.Size() != 2 || q3[1] != 2)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Clear", [this](auto& ls)
		{
			RingQueue<int> q(4);
			q.push(1);
			q.push(2);
			q.push(3);
			q.clear();

			if (!q.IsEmpty() || q.Size() != 0)
			{
				ls << "Clear failed" << lferr;
				return;
			}

			q.push(42);
			if (q.front() != 42 || q.Size() != 1)
			{
				ls << "Reuse after Clear failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		addTest("Wrap Around Full Cycle", [this](auto& ls)
		{
			RingQueue<int> q(4);

			for (int iter = 0; iter < 3; ++iter)
			{
				for (int i = 0; i < 4; ++i)
				{
					q.push(i + iter * 10);
				}

				for (int i = 0; i < 4; ++i)
				{
					auto v = q.pop();
					if (v != i + iter * 10)
					{
						ls << "Cycle " << iter << " position " << i << ": expected "
						   << (i + iter * 10) << ", got " << v << lferr;
						return;
					}
				}
			}

			ls << "Pass";
		});

		addTest("Performance vs std::queue", [this](auto& ls)
		{
			constexpr int Capacity = 4096;
			constexpr int NumIterations = 5000;

			time::TDuration heTime;
			time::TDuration stlTime;

			{
				time::ScopedTime measure(heTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					RingQueue<int> q(Capacity);
					for (int i = 0; i < Capacity; ++i)
					{
						q.push(i);
					}

					for (int i = 0; i < Capacity; ++i)
					{
						(void)q.pop();
					}
				}
			}

			{
				time::ScopedTime measure(stlTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					std::queue<int> q;
					for (int i = 0; i < Capacity; ++i)
					{
						q.push(i);
					}

					while (!q.empty())
					{
						q.pop();
					}
				}
			}

			ls << "RingQueue: " << heTime.count() << " us" << lf;
			ls << "std::queue (dynamic): " << stlTime.count() << " us" << lf;

			if (heTime > stlTime)
			{
				ls << "RingQueue is slower than std::queue (" << heTime.count()
				   << " vs " << stlTime.count() << ")" << lfwarn;
			}

			ls << "Pass";
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
