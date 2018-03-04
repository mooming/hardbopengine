#include "HardbopAllocator.h"

#ifdef __UNIT_TEST__

#include <iostream>
#include <vector>

bool HE::HardbopAllocatorTest::DoTest()
{
	using namespace std;

	for (int j = 0; j < 100; ++j)
	{
		vector<int, HardbopAllocator<int>> hbVector;
		for (int i = 0; i < 100; ++i)
		{
			hbVector.push_back(i);
		}

		for (auto value : hbVector)
		{
			cout << value << endl;
		}
	}

	return true;
}

#endif // __UNIT_TEST__
