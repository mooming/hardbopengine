// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Deque.h"

#ifdef __UNIT_TEST__
#include <deque>
#include "Core/ScopedTime.h"


namespace hbe
{

	void DequeTest::Prepare()
	{
		AddTest("Default Construction", [](auto&) { Deque<int> d; });

		AddTest("PushBack and PopBack", [this](auto& ls)
		{
			Deque<int> d;
			d.PushBack(1);
			d.PushBack(2);
			d.PushBack(3);

			if (d.Size() != 3 || d[0] != 1 || d[1] != 2 || d[2] != 3)
			{
				ls << "PushBack failed" << lferr;
				return;
			}

			d.PopBack();
			if (d.Size() != 2 || d.Back() != 2)
			{
				ls << "PopBack failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("PushFront and PopFront", [this](auto& ls)
		{
			Deque<int> d;
			d.PushFront(10);
			d.PushFront(20);
			d.PushFront(30);

			if (d.Size() != 3 || d[0] != 30 || d[1] != 20 || d[2] != 10)
			{
				ls << "PushFront failed" << lferr;
				return;
			}

			d.PopFront();
			if (d.Size() != 2 || d.Front() != 20)
			{
				ls << "PopFront failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Mixed Push", [this](auto& ls)
		{
			Deque<int> d;
			d.PushBack(2);
			d.PushFront(1);
			d.PushBack(3);

			if (d.Size() != 3 || d[0] != 1 || d[1] != 2 || d[2] != 3)
			{
				ls << "Expected 1,2,3 got " << d[0] << "," << d[1] << "," << d[2] << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Wrap Around", [this](auto& ls)
		{
			Deque<int> d;
			d.PushBack(1);
			d.PushBack(2);
			d.PushBack(3);
			d.PopFront();
			d.PopFront();
			d.PushBack(4);
			d.PushBack(5);

			if (d.Size() != 3 || d[0] != 3 || d[1] != 4 || d[2] != 5)
			{
				ls << "Expected 3,4,5 got " << d[0] << "," << d[1] << "," << d[2] << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Growth", [this](auto& ls)
		{
			Deque<int> d;
			int count = 100;

			for (int i = 0; i < count; ++i)
			{
				d.PushBack(i);
			}

			if (d.Size() != count)
			{
				ls << "Expected size " << count << ", got " << d.Size() << lferr;
				return;
			}

			for (int i = 0; i < count; ++i)
			{
				if (d[i] != i)
				{
					ls << "Mismatch at " << i << lferr;
					return;
				}
			}

			ls << "Pass";
		});

		AddTest("EmplaceBack", [this](auto& ls)
		{
			Deque<std::pair<int, int>> d;
			d.EmplaceBack(1, 2);
			d.EmplaceBack(3, 4);

			if (d.Size() != 2 || d[0].first != 1)
			{
				ls << "EmplaceBack failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Clear", [this](auto& ls)
		{
			Deque<int> d;
			d.PushBack(1);
			d.PushBack(2);
			d.Clear();

			if (!d.IsEmpty() || d.Size() != 0)
			{
				ls << "Clear failed" << lferr;
				return;
			}

			d.PushBack(10);
			if (d.Size() != 1 || d[0] != 10)
			{
				ls << "Reuse after Clear failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Move Semantics", [this](auto& ls)
		{
			Deque<int> d1;
			d1.PushBack(1);
			d1.PushBack(2);

			Deque<int> d2(std::move(d1));
			if (d2.Size() != 2 || d2[0] != 1)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			Deque<int> d3;
			d3 = std::move(d2);
			if (d3.Size() != 2 || d3[1] != 2)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Front and Back", [this](auto& ls)
		{
			Deque<int> d;
			d.PushBack(10);
			d.PushBack(20);
			d.PushBack(30);

			if (d.Front() != 10 || d.Back() != 30)
			{
				ls << "Front/Back mismatch" << lferr;
				return;
			}

			d.PopFront();
			d.PopBack();
			if (d.Front() != 20 || d.Back() != 20)
			{
				ls << "After pop Front/Back mismatch" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Reserve", [this](auto& ls)
		{
			Deque<int> d;
			d.PushBack(1);
			d.PushBack(2);
			d.Reserve(64);

			if (d.Capacity() < 64)
			{
				ls << "Expected capacity >= 64, got " << d.Capacity() << lferr;
				return;
			}

			if (d.Size() != 2 || d[0] != 1 || d[1] != 2)
			{
				ls << "Data corrupted after Reserve" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Performance vs std::deque", [this](auto& ls)
		{
			constexpr int NumItems = 50000;
			constexpr int NumIterations = 50;

			time::TDuration hePushTime;
			time::TDuration stlPushTime;
			time::TDuration hePopTime;
			time::TDuration stlPopTime;

			{
				time::ScopedTime measure(hePushTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					Deque<int> d;
					for (int i = 0; i < NumItems; ++i)
					{
						d.PushBack(i);
					}
				}
			}

			{
				time::ScopedTime measure(stlPushTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					std::deque<int> d;
					for (int i = 0; i < NumItems; ++i)
					{
						d.push_back(i);
					}
				}
			}

			{
				Deque<int> d;
				for (int i = 0; i < NumItems * NumIterations; ++i)
				{
					d.PushBack(i);
				}

				time::ScopedTime measure(hePopTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					for (int i = 0; i < NumItems; ++i)
					{
						d.PopFront();
					}
				}
			}

			{
				std::deque<int> d;
				for (int i = 0; i < NumItems * NumIterations; ++i)
				{
					d.push_back(i);
				}

				time::ScopedTime measure(stlPopTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					for (int i = 0; i < NumItems; ++i)
					{
						d.pop_front();
					}
				}
			}

			ls << "Deque push_back: " << hePushTime.count() << " us" << lf;
			ls << "std::deque push_back: " << stlPushTime.count() << " us" << lf;
			ls << "Deque pop_front: " << hePopTime.count() << " us" << lf;
			ls << "std::deque pop_front: " << stlPopTime.count() << " us" << lf;

			if (hePushTime > stlPushTime * 2)
			{
				ls << "Deque push_back is slower than 2x std::deque" << lfwarn;
			}

			ls << "Pass";
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
