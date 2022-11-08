// Created by mooming.go@gmail.com, 2017

#include "LinkedList.h"


using namespace HE;

#ifdef __UNIT_TEST__
#include "Memory/AllocatorScope.h"
#include "Memory/PoolAllocator.h"
#include "System/Debug.h"
#include "System/ScopedTime.h"
#include <list>


void LinkedListTest::Prepare()
{
#ifdef __DEBUG__
    const int COUNT = 4096;
#else //__DEBUG__
    const int COUNT = 8192 * 2;
#endif //__DEBUG__
    const int COUNT2 = 1024;
    
    AddTest("Iteration on the empty list", [this](auto& ls)
    {
        LinkedList<int> intList;
        
        for (auto value : intList)
        {
            ls << "It iterates a loop even if the list is empty. value = "
                << value << lferr;

            break;
        }
    });

    AddTest("Growth and Iteration", [this](auto& ls)
    {
        const auto NodeSize = sizeof(LinkedList<int>::Node);
        PoolAllocator<> alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
        AllocatorScope allocScope(alloc.GetID());
        
        Time::TDuration heTime;
        Time::TDuration stlTime;
        
        {
            Time::ScopedTime measure(heTime);
            
            LinkedList<int> intList;
            for (int i = 0; i < 100; ++i)
            {
                intList.Add(i);
            }
            
            int i = 0;
            for (auto value : intList)
            {
                if (value != i)
                {
                    ls << "Value Mismatched : value = " << value
                        << ", expected " << i << '.' << lferr;

                    return;
                }
                
                ++i;
            }
        }
        
        {
            Time::ScopedTime measure(stlTime);

            std::list<int> intList;
            for (int i = 0; i < COUNT; ++i)
            {
                intList.push_back(i);
            }

            int i = 0;
            for (auto value : intList)
            {
                if (value != i)
                {
                    ls << "Value Mismatched : value = " << value
                        << ", expected " << i << "." << lferr;

                    return;
                }

                ++i;
            }
        }
        
        ls << "Insert Time Compare : HE = " << Time::ToFloat(heTime)
            << ", STL = " << Time::ToFloat(stlTime) << lf;
        
        if (heTime > stlTime)
        {
            ls << "LinkedList is slower than the STL list" << std::endl
                << "HE = " << Time::ToFloat(heTime)
                << ", STL = " << Time::ToFloat(stlTime)
                << lfwarn;
        }
    });
    
    AddTest("Growth and Iteration", [this](auto& ls)
    {
        const auto NodeSize = sizeof(LinkedList<int>::Node);
        PoolAllocator<> alloc("LinkedListTest::Allocator", NodeSize, COUNT + 10);
        AllocatorScope allocScope(alloc.GetID());
        
        Time::TDuration heTime;
        Time::TDuration stlTime;
        
        long long stlValue = 0;
        long long heValue = 0;
        
        {
            std::list<int> intList;
            for (int i = 0; i < COUNT; ++i)
            {
                intList.push_back(i);
            }
            
            {
                Time::ScopedTime measure(stlTime);
                for (int i = 0; i < COUNT2; ++i)
                {
                    for (auto value : intList)
                    {
                        stlValue += value;
                    }
                }
            }
        }
        
        {
            LinkedList<int> intList;
            for (int i = 0; i < COUNT; ++i)
            {
                intList.Add(i);
            }
            
            {
                Time::ScopedTime measure(heTime);
                for (int i = 0; i < COUNT2; ++i)
                {
                    for (auto value : intList)
                    {
                        heValue += value;
                    }
                }
            }
        }
        
        if (heValue != stlValue)
        {
            ls << "Result Mismatched : HE = " << heValue
                << ", STL = " << stlValue << lferr;
            
            return;
        }
        
        ls << "Loop Time Compare : HE = " << Time::ToFloat(heTime)
            << ", STL = " << Time::ToFloat(stlTime) << lf;
        
        if (heTime > stlTime)
        {
            ls << "Lower Performance than STL list." << lfwarn;
        }
    });
}
#endif //__UNIT_TEST__
