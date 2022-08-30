// Created by mooming.go@gmail.com, 2017

#include "Array.h"


using namespace HE;

#ifdef __UNIT_TEST__
#include "Log/Logger.h"


bool ArrayTest::DoTest()
{
    TLog log(GetName());
    
    using namespace std;
    {
        const Array<int> array;
        if (array.Length() != 0)
        {
            log.OutError([&array](auto& ls)
            {
                ls << "Null Array Size is not zero. Size = " << array.Size();
            });
            
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
                log.OutError([i, value](auto& ls)
                {
                    ls << "Array Value Mismatch : " << value << " != " << i;
                });
                
                return false;
            }
            ++i;
        }

        if (i != 10)
        {
            log.OutError([i](auto& ls)
            {
                ls << "Array Size Mismatch : " << i << " != 10.";
            });
            
            return false;
        }

        array.Resize(5);

        i = 0;
        for (auto value : array)
        {
            if (value != i)
            {
                log.OutError([i, value](auto& ls)
                {
                    ls << "Array Value Mismatch : " << value << " != " << i;
                });
                
                return false;
            }
            ++i;
        }

        if (i != 5)
        {
            log.OutError([i](auto& ls)
            {
                ls << "Array Size Mismatch : " << i << " != 5.";
            });
            
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
                log.OutError([i, value](auto& ls)
                {
                    ls << "value = " << value << ", expected " << i;
                });
                
                return false;
            }

            ++i;
        }
    }

    return true;
}

#endif //__UNIT_TEST__
