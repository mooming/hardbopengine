// Created by mooming.go@gmail.com, 2022

#include "HUnorderedMap.h"

#ifdef __UNIT_TEST__
#include "String/StringUtil.h"

namespace hbe
{
	HUnorderedMapTest::HUnorderedMapTest() : TestCollection(StringUtil::ToCompactClassName(__PRETTY_FUNCTION__)) {}

	void HUnorderedMapTest::Prepare()
	{
		using namespace HSTL;

		AddTest("Default Construction", [](auto&) { HUnorderedMap<int, int> map; });

		AddTest("Insert Item", [this](auto& ls)
		{
			HUnorderedMap<int, int> map;
			map[1] = 2;
			map[2] = 1;

			ls << "map[1] = " << map[1] << lf;
			ls << "map[2] = " << map[2] << lf;
			ls << "map[3] = " << map[3] << lf;

			if (map[1] != 2)
			{
				ls << "Invalid value, map[1] = " << map[1] << ", but 2 is expected." << lferr;
			}

			if (map[2] != 1)
			{
				ls << "Invalid value, map[2] = " << map[2] << ", but 1 is expected." << lferr;
			}

			if (map[3] != 0)
			{
				ls << "Invalid value, map[3] = " << map[3] << ", but 0 is expected." << lferr;
			}
		});
	}

} // namespace hbe
#endif //__UNIT_TEST__
