// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "Vector2.h"

namespace HE
{
    template class Vector2<int>;
    template class Vector2<float>;
}

#ifdef __UNIT_TEST__

#include <iostream>

bool HE::Vector2Test::DoTest()
{
    using namespace std;

    cout << "Zero = " << Float2::Zero << endl;
    cout << "X = " << Float2::X << endl;
    cout << "Y = " << Float2::Y << endl;

    if ((Float2::X + Float2::Y) != Float2(1.0f, 1.0f))
    {
        cerr << "Float2 addition failed. X + Y = " << (Float2::X + Float2::Y) << endl;
        return false;
    }

    if (!Float2::Zero.IsZero())
    {
        cerr << "Float2 zero test failed. zero = " << Float2::Zero << endl;
        return false;
    }

    return true;
}

#endif //__UNIT_TEST__
