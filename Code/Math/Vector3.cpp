// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "Vector3.h"

namespace HE
{
    template class Vector3<int>;
    template class Vector3<float>;
}

#ifdef __UNIT_TEST__

#include "System/Time.h"
#include "System/Vector.h"
#include <iostream>

bool HE::Vector3Test::DoTest()
{
    using namespace std;

    cout << "Forward = " << Float3::Forward << endl;
    cout << "Right = " << Float3::Right << endl;
    cout << "Up = " << Float3::Up << endl;

    cout << "Zero = " << Float3::Zero << endl;
    cout << "Unity = " << Float3::Unity << endl;

    cout << "X x Y = " << Float3::X.Cross(Float3::Y) << endl;
    if (Float3::X.Cross(Float3::Y) != Float3::Z)
    {
        return false;
    }

#ifdef __LEFT_HANDED__
    cout << "Forward x Right = " << Float3::Forward.Cross(Float3::Right) << endl;
    if (Float3::Forward.Cross(Float3::Right) != Float3::Up)
    {
        return false;
    }
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
    cout << "Right x Forward = " << Float3::Right.Cross(Float3::Forward) << endl;
    if (Float3::Right.Cross(Float3::Forward) != Float3::Up)
    {
        return false;
    }
#endif //__RIGHT_HANDED__

    Float3 tmp;
    if (tmp != Float3::Zero)
    {
        cerr << "Default Float3 is not ZERO." << endl;
        return false;
    }

    Vector<Float3> vertices;
    for (int i = 0; i < 1000000; ++i)
    {
        const float x = static_cast<float>(i);
        vertices.New(x, 0.0f, 0.0f);
        vertices.New(0.0f, x, 0.0f);
        vertices.New(0.0f, 0.0f, x);
        vertices.New(x, x, 0.0f);
        vertices.New(0.0f, x, x);
        vertices.New(x, x, x);
    }

    {
        float heTime = 0.0f;
        float dotResult = 0.0f;

        {
            Time::MeasureSec measure(heTime);

            for (auto& vertex : vertices)
            {
                dotResult += tmp.Dot(vertex);
                tmp = vertex;
            }
        }

        cout << "Float3 Dot Time = " << heTime << ", Result = " << dotResult << endl;
    }

    return true;
}

#endif //__UNIT_TEST__
