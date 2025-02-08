// Created by mooming.go@gmail.com

#include "Vector2.h"

namespace HE
{
    template class Vector2<float>;
}

#ifdef __UNIT_TEST__

void HE::Vector2Test::Prepare()
{
    AddTest("Vector2 Constants & Operations", [this](auto& ls) {
        ls << "Zero = " << Float2::Zero << lf;
        ls << "X = " << Float2::X << lf;
        ls << "Y = " << Float2::Y << lf;

        if ((Float2::X + Float2::Y) != Float2(1.0f, 1.0f))
        {
            ls << "Float2 addition failed. X + Y = " << (Float2::X + Float2::Y)
               << lferr;
        }

        if (!Float2::Zero.IsZero())
        {
            ls << "Float2 zero test failed. zero = " << Float2::Zero << lferr;
        }
    });
}

#endif //__UNIT_TEST__
