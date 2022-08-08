// Created by mooming.go@gmail.com, 2022

#include "SystemAllocator.h"

#ifdef __UNIT_TEST__

#include <iostream>
#include <vector>


bool HE::SystemAllocatorTest::DoTest()
{
	using namespace std;

	for (int j = 0; j < 100; ++j)
	{
		vector<int, SystemAllocator<int>> hbVector;
		for (int i = 0; i < 100; ++i)
		{
			hbVector.push_back(i);
		}
	}

	return true;
}

#endif // __UNIT_TEST__
