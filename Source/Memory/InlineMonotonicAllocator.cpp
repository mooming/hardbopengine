// Created by mooming.go@gmail.com, 2022

#include "InlineMonotonicAllocator.h"


using namespace HE;

#ifdef __UNIT_TEST__
#include "AllocatorScope.h"
#include "HSTL/HVector.h"
#include "String/String.h"


void InlineMonotonicAllocatorTest::Prepare()
{
    using namespace std;
    using namespace HSTL;
    using Allocator = InlineMonotonicAllocator<2048>;

    AddTest("Vector Allocation", [this](auto& ls)
    {
        Allocator alloc("InlineMonotonicAllocator");

        {
            AllocatorScope scope(alloc.GetID());

            HVector<int> a;
            a.push_back(0);
        }

        if (alloc.GetUsage() != 0)
        {
            ls << "Deallocation Failed. Usage should be zero, but "
                << alloc.GetUsage() << lferr;
        }
    });

    AddTest("Allocation (2)", [this](auto& ls)
    {
        Allocator alloc("InlineMonotonicAllocator");

        {
            AllocatorScope scope(alloc.GetID());

            HVector<int> a;
            a.push_back(0);

            HVector<int> b;
            b.push_back(1);
        }

        if (alloc.GetUsage() != 0)
        {
            ls << "Deallocation Failed. Usage should be zero, but "
                << alloc.GetUsage() << lferr;
        }
    });


    AddTest("Deallocation", [this](auto& ls)
    {
        Allocator alloc("InlineMonotonicAllocator");
        AllocatorScope scope(alloc.GetID());

        {
            String a = "0";
        }

        if (alloc.GetUsage() > 0)
        {
            ls << "Deallocation not ignored. Usage should not be zero, but "
                << alloc.GetUsage() << lferr;
        }
    });

    AddTest("Deallocation (2)", [this](auto& ls)
    {
        Allocator alloc("InlineMonotonicAllocator");
        AllocatorScope scope(alloc.GetID());

        {
            String a = "0";
            String b = "1";
        }

        if (alloc.GetUsage() > 0)
        {
            ls << "Deallocation is not ignored. Usage should not be zero, but "
                << alloc.GetUsage() << lferr;
        }
    });
}

#endif //__UNIT_TEST__

