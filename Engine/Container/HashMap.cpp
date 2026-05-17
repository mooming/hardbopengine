// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "HashMap.h"

#ifdef __UNIT_TEST__
#include <unordered_map>
#include "Core/ScopedTime.h"


namespace hbe
{

	void HashMapTest::Prepare()
	{
		AddTest("Default Construction", [](auto&) { HashMap<int, int> m; });

		AddTest("Insert and Access", [this](auto& ls)
		{
			HashMap<int, int> m;
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

		AddTest("Insert Return Value", [this](auto& ls)
		{
			HashMap<int, int> m;
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
			HashMap<int, int> m;
			m[1] = 10;
			m[2] = 20;
			m[3] = 30;

			FatalAssert(m.Remove(2));
			FatalAssert(!m.Remove(99));

			if (m.Size() != 2 || m.Contains(2))
			{
				ls << "Remove failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Contains", [this](auto& ls)
		{
			HashMap<int, int> m;
			m[5] = 50;

			if (!m.Contains(5) || m.Contains(99))
			{
				ls << "Contains check failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Find", [this](auto& ls)
		{
			HashMap<int, int> m;
			m[10] = 100;
			m[20] = 200;

			auto it = m.Find(10);
			if (it == m.end() || it->value != 100)
			{
				ls << "Find(10) failed" << lferr;
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

		AddTest("Growth and Rehash", [this](auto& ls)
		{
			HashMap<int, int> m;
			int count = 1000;

			for (int i = 0; i < count; ++i)
			{
				m[i] = i * 2;
			}

			if (m.Size() != count)
			{
				ls << "Expected size " << count << ", got " << m.Size() << lferr;
				return;
			}

			for (int i = 0; i < count; ++i)
			{
				if (m[i] != i * 2)
				{
					ls << "Mismatch at " << i << lferr;
					return;
				}
			}

			ls << "Pass";
		});

		AddTest("Tombstone Reuse", [this](auto& ls)
		{
			HashMap<int, int> m;
			m[1] = 10;
			m[2] = 20;
			m[3] = 30;
			m.Remove(2);
			m[2] = 200;

			if (!m.Contains(2) || m[2] != 200)
			{
				ls << "Tombstone reuse failed" << lferr;
				return;
			}

			if (m.Size() != 3)
			{
				ls << "Expected size 3 after tombstone reuse" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Iteration", [this](auto& ls)
		{
			HashMap<int, int> m;
			m[1] = 10;
			m[2] = 20;
			m[3] = 30;

			int count = 0;
			int sum = 0;
			for (auto& pair : m)
			{
				sum += pair.key + pair.value;
				++count;
			}

			if (count != 3 || sum != (1 + 2 + 3) + (10 + 20 + 30))
			{
				ls << "Iteration failed: count=" << count << " sum=" << sum << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Clear", [this](auto& ls)
		{
			HashMap<int, int> m;
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
			HashMap<int, int> m1;
			m1[1] = 10;
			m1[2] = 20;

			HashMap<int, int> m2(std::move(m1));
			if (m2.Size() != 2 || m2[1] != 10)
			{
				ls << "Move constructor failed" << lferr;
				return;
			}

			HashMap<int, int> m3;
			m3 = std::move(m2);
			if (m3.Size() != 2 || m3[2] != 20)
			{
				ls << "Move assignment failed" << lferr;
				return;
			}

			ls << "Pass";
		});

		AddTest("Performance vs std::unordered_map", [this](auto& ls)
		{
			constexpr int NumItems = 50000;

			time::TDuration heInsertTime;
			time::TDuration stlInsertTime;
			time::TDuration heFindTime;
			time::TDuration stlFindTime;

			{
				time::ScopedTime measure(heInsertTime);
				HashMap<int, int> m;
				for (int i = 0; i < NumItems; ++i)
				{
					m[i] = i * 2;
				}
			}

			{
				time::ScopedTime measure(stlInsertTime);
				std::unordered_map<int, int> m;
				for (int i = 0; i < NumItems; ++i)
				{
					m[i] = i * 2;
				}
			}

			{
				HashMap<int, int> m;
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
				std::unordered_map<int, int> m;
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

			ls << "HashMap insert: " << heInsertTime.count() << " us" << lf;
			ls << "std::unordered_map insert: " << stlInsertTime.count() << " us" << lf;
			ls << "HashMap find: " << heFindTime.count() << " us" << lf;
			ls << "std::unordered_map find: " << stlFindTime.count() << " us" << lf;

			if (heInsertTime > stlInsertTime * 2)
			{
				ls << "HashMap insert is slower than 2x std::unordered_map" << lfwarn;
			}

			ls << "Pass";
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
