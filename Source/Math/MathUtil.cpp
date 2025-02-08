// Created by mooming.go@gmail.com

#include "MathUtil.h"

#ifdef __UNIT_TEST__

void HE::MathUtilTest::Prepare()
{
    AddTest("Exponent", [this](auto& ls) {
        ls << "2^10 = " << Pow(2, 10);

        if (Pow(2, 10) != 1024)
        {
            ls << "Pow(int, int) result failed = " << Pow(2, 10)
               << ", but 1024 expected." << lferr;
        }

        ls << "2.0f^10.0f = " << Pow(2.0f, 10.0f);

        if (Pow(2, 10) != Pow(2.0f, 10.0f))
        {
            ls << "Pow(float, float) result failed = " << Pow(2.0f, 10.0f)
               << ", but 1024.0f expected." << lferr;
        }
    });
}

#endif //__UNIT_TEST__
