// Created by mooming.go@gmail.com, 2017

#include "PoolAllocator.h"


using namespace HE;


#ifdef __UNIT_TEST__

void PoolAllocatorTest::Prepare()
{
    AddTest("Construction", [](auto&)
    {
        for (int i = 1; i < 100; ++i)
        {
            PoolAllocator<> pool("TestPoolAllocator", i, 100);
        }
    });


    AddTest("Allocation & Deallocation", [this](auto& ls)
    {
        PoolAllocator<> pool("TestPoolAllocator", 4096, 100);

        for (int i = 0; i < 100; ++i)
        {
            auto ptr = pool.Allocate(50);
            auto size = pool.GetSize(ptr);

            if (size != 4096)
            {
                ls << "The size is incorrect. " << size
                    << ", but 100 expected." << lferr;
                break;
            }

            pool.Deallocate(ptr);
        }
    });
}

#endif //__UNIT_TEST__
