// Created by mooming.go@gmail.com

#include "Optional.h"

#ifdef __UNIT_TEST__

void HE::OptionalTest::Prepare()
{
    AddTest("Optional<int>", [this](auto& ls) {
        Optional<int> a;
        if (a)
        {
            ls << "Uninitialized optional SHOULD BE false." << lferr;
        }

        a = 10;

        if (!a)
        {
            ls << "Initialized optional SHOULD BE true." << lferr;
        }

        if (*a != 10)
        {
            ls << "Optional value = " << *a << ", but expected 10." << lferr;
        }
    });

    AddTest("Optional<int&>", [this](auto& ls) {
        Optional<int&> a;
        if (a)
        {
            ls << "Uninitialized optional SHOULD BE false." << lferr;
        }

        int x = 10;
        a = x;

        if (!a)
        {
            ls << "Initialized optional SHOULD BE true." << lferr;
        }

        if (*a != 10)
        {
            ls << "Optional value = " << *a << ", but expected 10." << lferr;
        }

        auto& aValue = a.Value();
        if (&aValue != &x)
        {
            ls << "Optional<ref> value address is incorrect! ptr = "
               << (void*)(&aValue) << ", but " << (void*)(&x) << " expected."
               << lferr;
        }
    });

    AddTest("Optional<Class&>", [this](auto& ls) {
        static int conCount = 0;
        static int desCount = 0;

        struct Integer
        {
            int x;

            Integer()
                : x(conCount++)
            {
            }

            ~Integer() { ++desCount; }
        };

        Optional<Integer&> a;
        if (a)
        {
            ls << "Uninitialized optional SHOULD BE false." << lferr;
        }

        if (conCount != 0)
        {
            ls << "Uninitialized ref. type optional SHOULD NOT instance the "
                  "class."
               << lferr;
        }

        if (desCount != 0)
        {
            ls << "Uninitialized ref. type optional SHOULD NOT destroy the "
                  "instance."
               << lferr;
        }

        Integer x, y;
        a = y;

        if (conCount != 2)
        {
            ls << "Constructing Count = " << conCount << ", but expected 1"
               << lferr;
        }

        if (desCount != 0)
        {
            ls << "Destructing Count = " << desCount << ", but expected 0"
               << lferr;
        }

        if (!a)
        {
            ls << "Initialized optional SHOULD BE true." << lferr;
        }

        if (a.Value().x != 1)
        {
            ls << "Optional value = " << a.Value().x << ", but expected 0."
               << lferr;
        }
    });
}

#endif //__UNIT_TEST__
