// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "Vector4.h"

namespace HE
{
    template class Vector4<float>;
} // HE

#ifdef __UNIT_TEST__

#include "Container/Vector.h"
#include "System/Time.h"
#include <iostream>


bool HE::Vector4Test::DoTest()
{
    using namespace std;

    cout << "Forward = " << Float4::Forward << endl;
    cout << "Right = " << Float4::Right << endl;
    cout << "Up = " << Float4::Up << endl;

    cout << "Zero = " << Float4::Zero << endl;
    cout << "Unity = " << Float4::Unity << endl;

    Float4 tmp;
    if (tmp != Float4::Zero)
    {
        cerr << "Default Float4 is not ZERO. Default = " << tmp << endl;
        return false;
    }

    Vector<Float4> vertices;
    for (int i = 0; i < 1000000; ++i)
    {
        const float x = static_cast<float>(i);
        vertices.emplace_back(x, 0.0f, 0.0f);
        vertices.emplace_back(0.0f, x, 0.0f);
        vertices.emplace_back(0.0f, 0.0f, x);
        vertices.emplace_back(x, x, 0.0f);
        vertices.emplace_back(0.0f, x, x);
        vertices.emplace_back(x, x, x);
    }

    {
        Time::TDuration heTime;
        float dotResult = 0.0f;

        {
            Time::Measure measure(heTime);

            for (auto& vertex : vertices)
            {
                dotResult += tmp.Dot(vertex);
                tmp = vertex;
            }
        }

        cout << "Float4 Dot Time = " << Time::ToMilliSec<float>(heTime)
            << ", Result = " << dotResult << endl;
    }

    return true;
}

#endif //__UNIT_TEST__
