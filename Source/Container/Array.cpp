// Created by mooming.go@gmail.com

#include "Array.h"

#ifdef __UNIT_TEST__
#include "Log/Logger.h"

namespace HE
{

    void ArrayTest::Prepare()
    {
        AddTest("Default Constructor", [this](auto &ls) {
            const Array<int> array;
            if (array.Size() != 0)
            {
                ls << "Null Array Size is not zero. Size = " << array.Size()
                   << lferr;
            }
        });

        AddTest("Initializer List", [this](auto &ls) {
            Array<int> array({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

            int i = 0;
            for (auto value : array)
            {
                if (value != i)
                {
                    ls << "Array Value Mismatch : " << value << " != " << i
                       << lferr;
                    break;
                }

                ++i;
            }

            if (i != 10)
            {
                ls << "Array Size Mismatch : " << i << " != 10." << lferr;
            }
        });

        AddTest("Initializer List (2)", [this](auto &ls) {
            const Array<int> array({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
            int i = 0;

            for (auto value : array)
            {
                if (value != i)
                {
                    ls << "value = " << value << ", expected " << i << lferr;
                    break;
                }

                ++i;
            }
        });
    }

} // namespace HE
#endif //__UNIT_TEST__
