// Created by mooming.go@gmail.com, 2017

#include "System/Optional.h"

#ifdef __UNIT_TEST__

#include <iostream>

bool HE::OptionalTest::DoTest()
{
    using namespace std;
    using namespace HE;

    {
        Optional<int> a;
        if (a)
        {
            cout << "Uninitialized optional SHOULD BE false." << endl;
            return false;
        }

        a = 10;

        if (!a)
        {
            cout << "Initialized optional SHOULD BE true." << endl;
            return false;
        }

        if (*a != 10)
        {
            cout << "Optional value = " << *a << ", but expected 10." << endl;
            return false;
        }
    }

    {
        Optional<int&> a;
        if (a)
        {
            cout << "Uninitialized optional SHOULD BE false." << endl;
            return false;
        }

        int x = 10;
        a = x;

        if (!a)
        {
            cout << "Initialized optional SHOULD BE true." << endl;
            return false;
        }

        if (*a != 10)
        {
            cout << "Optional value = " << *a << ", but expected 10." << endl;
            return false;
        }
    }

    {
        static int conCount = 0;
        static int desCount = 0;

        class Integer
        {
        public:
            int x;

            Integer() : x(conCount++)
            {
            }

            ~Integer()
            {
                ++desCount;
            }
        };

        Optional<Integer&> a;
        if (a)
        {
            cout << "Uninitialized optional SHOULD BE false." << endl;
            return false;
        }

        if (conCount != 0)
        {
            cout << "Uninitialized ref. type optional SHOULD NOT instance the class." << endl;
            return false;
        }

        if (desCount != 0)
        {
            cout << "Uninitialized ref. type optional SHOULD NOT destroy the instance." << endl;
            return false;
        }

        Integer x, y;
        a = y;

        if (conCount != 2)
        {
            cout << "Constructing Count = " << conCount << ", but expected 1" << endl;
            return false;
        }

        if (desCount != 0)
        {
            cout << "Destructing Count = " << desCount << ", but expected 0" << endl;
            return false;
        }

        if (!a)
        {
            cout << "Initialized optional SHOULD BE true." << endl;
            return false;
        }

        if (a.Value().x != 1)
        {
            cout << "Optional value = " << a.Value().x << ", but expected 0." << endl;
            return false;
        }
    }


    return true;
}

#endif //__UNIT_TEST__
