// Created by mooming.go@gmail.com, 2017

#include "BaseAllocator.h"

#ifdef __UNIT_TEST__
#include <vector>


void HE::BaseAllocatorTest::Prepare()
{
    AddTest("Multiple Allocations", [](auto&)
    {
        for (int j = 0; j < 100; ++j)
        {
            std::vector<int, BaseAllocator<int>> hbVector;
            for (int i = 0; i < 2048; ++i)
            {
                hbVector.push_back(i);
            }
        }
    });
}

#endif // __UNIT_TEST__
