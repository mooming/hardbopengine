// Created by mooming.go@gmail.com, 2017

#include "System/Vector.h"

using namespace HE;

#ifdef __UNIT_TEST__
#include "System/StackAllocator.h"
#include "System/String.h"
#include "System/Time.h"

#include <vector>
#include <iostream>

bool VectorTest::DoTest()
{
    using namespace std;

    StackAllocator stack(512 * 1024 * 1024);
    AllocatorScope stackScope(stack.Id());

#ifdef __DEBUG__
    const int COUNT = 20;
    const int COUNT2 = 16;
#else
    const int COUNT = 100000;
    const int COUNT2 = 1024;
#endif // __DEBUG__

    {
        Vector<int> intList;

        int total = 0;
        for (auto value : intList)
        {
            total += value;
        }
    }

    {
        vector<String> strList;

        for (int i = 0; i < COUNT; ++i)
        {
            strList.push_back(String(i));
        }

        int expValue = 0;
        for (auto value : strList)
        {
            if (value != String(expValue))
            {
                cerr << "Value Mismatched. value = " << value << ", expceted = " << expValue << endl;
                return false;
            }

            ++expValue;
        }
    }

    {
        Vector<String> strList;
        for (int i = 0; i < COUNT; ++i)
        {
            strList.push_back(String(i));
        }

        int expValue = 0;
        for (auto value : strList)
        {
            if (value != String(expValue))
            {
                cerr << "Value Mismatched. value = " << value << ", expected = " << expValue << endl;
                return false;
            }

            ++expValue;
        }
    }

    {
        Vector<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.push_back(i);
        }

        int expValue = 0;
        for (auto value : intList)
        {
            if (value != expValue)
            {
                return false;
            }

            ++expValue;
        }
    }

    float heTime = 0.0f;
    float stlTime = 0.0f;

    {
        Time::MeasureSec measure(heTime);

        Vector<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.push_back(i);
        }

        for (int i = 0; i < COUNT; ++i)
        {
            if (intList[i] != i)
            {
                cout << "Value Mismatched : value = " << intList[i]
                    << ", expected " << i << "." << endl;
                return false;
            }
        }
    }

    {
        Time::MeasureSec measure(stlTime);

        vector<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.push_back(i);
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
        vector<int> intList;
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
        Vector<int> intList;
        for (int i = 0; i < COUNT; ++i)
        {
            intList.push_back(i);
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
