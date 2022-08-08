// Created by mooming.go@gmail.com, 2017

#include "BaseAllocator.h"

#ifdef __UNIT_TEST__

#include <iostream>
#include <vector>


bool HE::BaseAllocatorTest::DoTest()
{
	using namespace std;

	for (int j = 0; j < 100; ++j)
	{
		vector<int, BaseAllocator<int>> hbVector;
		for (int i = 0; i < 2048; ++i)
		{
			hbVector.push_back(i);
		}
	}

	return true;
}

#endif // __UNIT_TEST__
