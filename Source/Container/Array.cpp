// Created by mooming.go@gmail.com, 2017

#include "Array.h"


using namespace HE;

#ifdef __UNIT_TEST__
#include <iostream>

bool ArrayTest::DoTest()
{
    using namespace std;
    {
        const Array<int> array;
        if (array.Length() != 0)
        {
            cout << "Null Array Size is not zero. Size = " << array.Size() << endl;
            return false;
        }
    }

    {
        Array<int> array({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });
        int i = 0;
        for (auto value : array)
        {
            if (value != i)
            {
                cout << "Array Value Mismatch : " << value << " != " << i << endl;
                return false;
            }
            ++i;
        }

        if (i != 10)
        {
            cout << "Array Size Mismatch : " << i << " != 10." << endl;
            return false;
        }

        array.Resize(5);

        i = 0;
        for (auto value : array)
        {
            if (value != i)
            {
                cout << "Array Value Mismatch : " << value << " != " << i << endl;
                return false;
            }
            ++i;
        }

        if (i != 5)
        {
            cout << "Array Size Mismatch : " << i << " != 5." << endl;
            return false;
        }
    }

    {
        const Array<int> array({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 });
        int i = 0;

        for (auto value : array)
        {
            if (value != i)
            {
                cout << "value = " << value << ", expected " << i  << endl;
                return false;
            }

            ++i;
        }
    }

    return true;
}

#endif //__UNIT_TEST__
