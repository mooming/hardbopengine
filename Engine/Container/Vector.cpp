// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Vector.h"

#ifdef __UNIT_TEST__
#include <vector>
#include "Core/ScopedTime.h"

namespace hbe
{

	void VectorTest::Prepare()
	{
		AddTest("Default Construction", [](auto&) { Vector<int> v; });

		AddTest("PushBack and Access", [this](auto& ls)
		{
			Vector<int> v;
			v.PushBack(10);
			v.PushBack(20);
			v.PushBack(30);

			if (v.Size() != 3)
			{
				ls << "Expected size 3, got " << v.Size() << lferr;
				return;
			}

			if (v[0] != 10 || v[1] != 20 || v[2] != 30)
			{
				ls << "Unexpected values" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("PopBack", [this](auto& ls)
		{
			Vector<int> v;
			v.PushBack(1);
			v.PushBack(2);
			v.PushBack(3);
			v.PopBack();

			if (v.Size() != 2 || v.Back() != 2)
			{
				ls << "PopBack failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Front and Back", [this](auto& ls)
		{
			Vector<int> v;
			v.PushBack(10);
			v.PushBack(20);

			if (v.Front() != 10 || v.Back() != 20)
			{
				ls << "Front/Back mismatch" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Initializer List", [this](auto& ls)
		{
			Vector<int> v = {1, 2, 3, 4, 5};

			if (v.Size() != 5)
			{
				ls << "Expected size 5, got " << v.Size() << lferr;
				return;
			}

			for (int i = 0; i < 5; ++i)
			{
				if (v[i] != i + 1)
				{
					ls << "Mismatch at " << i << lferr;
					return;
				}
			}

			ls << "Pass";
		});

		AddTest("Growth", [this](auto& ls)
		{
			Vector<int> v;
			int count = 1000;

			for (int i = 0; i < count; ++i)
			{
				v.PushBack(i);
			}

			if (v.Size() != count)
			{
				ls << "Expected size " << count << ", got " << v.Size() << lferr;
				return;
			}

			for (int i = 0; i < count; ++i)
			{
				if (v[i] != i)
				{
					ls << "Mismatch at " << i << lferr;
					return;
				}
			}

			ls << "Pass";
		});

		AddTest("Reserve", [this](auto& ls)
		{
			Vector<int> v;
			v.Reserve(64);

			if (v.Capacity() < 64)
			{
				ls << "Expected capacity >= 64, got " << v.Capacity() << lferr;
				return;
			}

			v.PushBack(42);
			if (v[0] != 42)
			{
				ls << "Data corrupted after Reserve" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("EmplaceBack", [this](auto& ls)
		{
			Vector<std::pair<int, int>> v;
			v.EmplaceBack(1, 2);
			v.EmplaceBack(3, 4);

			if (v.Size() != 2)
			{
				ls << "Expected size 2" << lferr;
				return;
			}

			if (v[0].first != 1 || v[0].second != 2)
			{
				ls << "EmplaceBack failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Clear", [this](auto& ls)
		{
			Vector<int> v;
			v.PushBack(1);
			v.PushBack(2);
			v.Clear();

			if (!v.IsEmpty())
			{
				ls << "Should be empty after Clear" << lferr;
				return;
			}

			v.PushBack(10);
			if (v.Size() != 1 || v[0] != 10)
			{
				ls << "Reuse after Clear failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Move Semantics", [this](auto& ls)
		{
			Vector<int> v1;
			v1.PushBack(1);
			v1.PushBack(2);

			Vector<int> v2(std::move(v1));
			if (v2.Size() != 2 || v2[0] != 1)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			Vector<int> v3;
			v3 = std::move(v2);
			if (v3.Size() != 2 || v3[0] != 1)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("FindIndex", [this](auto& ls)
		{
			Vector<int> v;
			v.PushBack(10);
			v.PushBack(20);
			v.PushBack(30);

			if (v.FindIndex(20) != 1)
			{
				ls << "FindIndex(20) should be 1" << lferr;
				return;
			}

			if (v.FindIndex(99) != -1)
			{
				ls << "FindIndex(99) should be -1" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Performance vs std::vector", [this](auto& ls)
		{
			constexpr int NumItems = 100000;
			constexpr int NumIterations = 30;

			time::TDuration heTime;
			time::TDuration stlTime;

			{
				time::ScopedTime measure(heTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					Vector<int> v;
					for (int i = 0; i < NumItems; ++i)
					{
						v.PushBack(i);
					}
				}
			}

			{
				time::ScopedTime measure(stlTime);
				for (int iter = 0; iter < NumIterations; ++iter)
				{
					std::vector<int> v;
					for (int i = 0; i < NumItems; ++i)
					{
						v.push_back(i);
					}
				}
			}

			ls << "Vector push_back: " << heTime.count() << " us" << lf;
			ls << "std::vector push_back: " << stlTime.count() << " us" << lf;

			if (heTime > stlTime * 2)
			{
				ls << "Vector push_back is slower than 2x std::vector" << lfwarn;
			}

			ls << "Pass";
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
