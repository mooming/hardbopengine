// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Map.h"

#ifdef __UNIT_TEST__
#include <map>
#include "Core/ScopedTime.h"

namespace hbe
{

	void MapTest::Prepare()
	{
		AddTest("Default Construction", [](auto&) { Map<int, int> m; });

		AddTest("Insert and Access", [this](auto& ls)
		{
			Map<int, int> m;
			m[1] = 10;
			m[2] = 20;
			m[3] = 30;

			if (m[1] != 10 || m[2] != 20 || m[3] != 30)
			{
				ls << "Insert/Access failed" << lferr;
				return;
			}

			if (m.Size() != 3)
			{
				ls << "Expected size 3, got " << m.Size() << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Sorted Order", [this](auto& ls)
		{
			Map<int, int> m;
			m[3] = 30;
			m[1] = 10;
			m[2] = 20;

			int expected = 1;
			for (auto& pair : m)
			{
				if (pair.key != expected)
				{
					ls << "Expected key " << expected << ", got " << pair.key << lferr;
					return;
				}

				++expected;
			}

			ls << "Pass";
		});

		AddTest("Find", [this](auto& ls)
		{
			Map<int, int> m;
			m[5] = 50;
			m[3] = 30;

			auto it = m.Find(5);
			if (it == m.end() || it->value != 50)
			{
				ls << "Find(5) failed" << lferr;
				return;
			}

			it = m.Find(99);
			if (it != m.end())
			{
				ls << "Find(99) should return end()" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Insert Return Value", [this](auto& ls)
		{
			Map<int, int> m;
			FatalAssert(m.Insert(1, 10));
			FatalAssert(!m.Insert(1, 20));

			if (m[1] != 10)
			{
				ls << "Value should remain 10 after failed duplicate insert" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Remove", [this](auto& ls)
		{
			Map<int, int> m;
			m[1] = 10;
			m[2] = 20;
			m[3] = 30;

			FatalAssert(m.Remove(2));
			FatalAssert(!m.Remove(99));

			if (m.Size() != 2)
			{
				ls << "Expected size 2, got " << m.Size() << lferr;
				return;
			}

			if (m.Contains(2))
			{
				ls << "Removed key should not exist" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Contains", [this](auto& ls)
		{
			Map<int, int> m;
			m[5] = 50;

			if (!m.Contains(5) || m.Contains(99))
			{
				ls << "Contains check failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Clear", [this](auto& ls)
		{
			Map<int, int> m;
			m[1] = 10;
			m[2] = 20;
			m.Clear();

			if (!m.IsEmpty() || m.Size() != 0)
			{
				ls << "Clear failed" << lferr;
				return;
			}

			m[3] = 30;
			if (m.Size() != 1 || m[3] != 30)
			{
				ls << "Reuse after Clear failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Move Semantics", [this](auto& ls)
		{
			Map<int, int> m1;
			m1[1] = 10;
			m1[2] = 20;

			Map<int, int> m2(std::move(m1));
			if (m2.Size() != 2 || m2[1] != 10)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			Map<int, int> m3;
			m3 = std::move(m2);
			if (m3.Size() != 2 || m3[2] != 20)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Performance vs std::map", [this](auto& ls)
		{
			constexpr int NumItems = 10000;

			time::TDuration heInsertTime;
			time::TDuration stlInsertTime;
			time::TDuration heFindTime;
			time::TDuration stlFindTime;

			{
				time::ScopedTime measure(heInsertTime);
				Map<int, int> m;
				for (int i = 0; i < NumItems; ++i)
				{
					m[i] = i * 2;
				}
			}

			{
				time::ScopedTime measure(stlInsertTime);
				std::map<int, int> m;
				for (int i = 0; i < NumItems; ++i)
				{
					m[i] = i * 2;
				}
			}

			{
				Map<int, int> m;
				for (int i = 0; i < NumItems; ++i)
				{
					m[i] = i;
				}

				volatile int sum = 0;
				time::ScopedTime measure(heFindTime);
				for (int i = 0; i < NumItems; ++i)
				{
					auto it = m.Find(i);
					if (it != m.end())
					{
						sum += it->value;
					}
				}
			}

			{
				std::map<int, int> m;
				for (int i = 0; i < NumItems; ++i)
				{
					m[i] = i;
				}

				volatile int sum = 0;
				time::ScopedTime measure(stlFindTime);
				for (int i = 0; i < NumItems; ++i)
				{
					auto it = m.find(i);
					if (it != m.end())
					{
						sum += it->second;
					}
				}
			}

			ls << "Map insert (sorted vector): " << heInsertTime.count() << " us" << lf;
			ls << "std::map insert (RB tree): " << stlInsertTime.count() << " us" << lf;
			ls << "Map find (binary search): " << heFindTime.count() << " us" << lf;
			ls << "std::map find (RB tree): " << stlFindTime.count() << " us" << lf;

			if (heInsertTime > stlInsertTime * 3)
			{
				ls << "Map insert is slower than 3x std::map (expected for sorted vector)" << lfwarn;
			}

			ls << "Pass";
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
