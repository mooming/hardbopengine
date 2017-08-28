// Copyright Hansol Park (mooming.go@gmail.com). All rights reserved.

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
    
    {
        auto x = Float3::X;
        auto y = Float3::Y;
        
        if (x != Float3::Slerp(x, y, 0.0f))
        {
            cerr << "Float3 slerp 1 should match its origin " << x
                << ", but " << Float3::Slerp(x, y, 0.0f);
            return false;
        }
        
        if (y != Float3::Slerp(x, y, 1.0f))
        {
            cerr << "Float3 slerp 1 should match its destination " << y
                << ", but " << Float3::Slerp(x, y, 1.0f);
            return false;
        }
        
        Float3 half = Float3::Slerp(x, y, 0.5f);
        cout << "Float3, Slerp half = " << half << endl;
        
        if (!IsEqual(half.Length(), 1.0f))
        {
            cerr << "Float3 slerp of X and Y with 0.5f should have length 1 "
                << ", but " << half.Length();
            return false;
        }
    }

    return true;
}

#endif //__UNIT_TEST__
