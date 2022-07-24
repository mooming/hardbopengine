// Created by mooming.go@gmail.com, 2017

#include "System/LinkedList.h"

using namespace HE;

#ifdef __UNIT_TEST__
#include "Time.h"

#include "StackAllocator.h"

#include <list>
#include <iostream>

bool LinkedListTest::DoTest()
{
    using namespace std;

    StackAllocator stack(512 * 1024 * 1024);
    AllocatorScope stackScope(stack.Id());

#ifdef __DEBUG__
    const int COUNT = 4096;
#else //__DEBUG__
    const int COUNT = 100000;
#endif //__DEBUG__
    const int COUNT2 = 1024;
    {
        LinkedList<int> intList;

        int total = 0;
        for (auto value : intList)
        {
            total += value;
        }
    }

    float heTime = 0.0f;
    float stlTime = 0.0f;

    {
        Time::MeasureSec measure(heTime);

        LinkedList<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.Add(i);
        }

        int i = 0;
        for (auto value : intList)
        {
            if (value != i)
            {
                cout << "Value Mismatched : value = " << value
                    << ", expected " << i << "." << endl;
                return false;
            }

            ++i;
        }
    }

    {
        Time::MeasureSec measure(stlTime);

        list<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.push_back(i);
        }

        int i = 0;
        for (auto value : intList)
        {
            if (value != i)
            {
                cout << "Value Mismatched : value = " << value
                    << ", expected " << i << "." << endl;
                return false;
            }

            ++i;
        }
    }

    cout << "Insert Time Compare : HE = " << heTime << ", STL = " << stlTime << endl;
    if (heTime > stlTime)
    {
        cerr << "Lower Performance than STL list." << endl;
    }

    long long stlValue = 0;
    long long heValue = 0;

    {
        list<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.push_back(i);
        }

        {
            Time::MeasureSec measure(stlTime);
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
            Time::MeasureSec measure(heTime);
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
        cout << "Result Mismatched : HE = " << heValue << ", STL = " << stlValue << endl;
        return false;
    }

    cout << "Loop Time Compare : HE = " << heTime << ", STL = " << stlTime << endl;
    if (heTime > stlTime)
    {
        cerr << "Lower Performance than STL list." << endl;
    }

    return true;
}
#endif //__UNIT_TEST__
