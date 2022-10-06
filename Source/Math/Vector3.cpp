// Copyright Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Vector3.h"

namespace HE
{
    template class Vector3<float>;
} // HE

#ifdef __UNIT_TEST__
#include "System/Time.h"
#include "HSTL/HVector.h"


void HE::Vector3Test::Prepare()
{
    AddTest("Constants", [this](auto& ls)
    {
        ls << "Forward = " << Float3::Forward << lf;
        ls << "Right = " << Float3::Right << lf;
        ls << "Up = " << Float3::Up << lf;

        ls << "Zero = " << Float3::Zero << lf;
        ls << "Unity = " << Float3::Unity << lf;
    });

    AddTest("Cross Product", [this](auto& ls)
    {
        auto xCrossY = Float3::X.Cross(Float3::Y);
        ls << "X x Y = " << xCrossY << lf;

        if (xCrossY != Float3::Z)
        {
            ls << "Incorrect Cross Product Result: X x Y = "
                << xCrossY << ", but " << Float3::Z << " expected." << lferr;
        }

#ifdef __LEFT_HANDED__
        auto fCrossR = Float3::Forward.Cross(Float3::Right);
        ls << "Forward x Right = " << fCrossR << lf;

        if (fCrossR != Float3::Up)
        {
            ls << "Incorrect Cross Product Result: Forward x Right = "
                << fCrossR << ", but " << Float3::Up << " expected." << lferr;
        }
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
        auto rCrossF = Float3::Right.Cross(Float3::Forward);
        ls << "Right x Forward = " << rCrossF << lf;

        if (rCrossF != Float3::Up)
        {
            ls << "Incorrect Cross Product Result: Right x Forward = "
                << rCrossF << ", but " << Float3::Up << " expected." << lferr;
        }
#endif //__RIGHT_HANDED__
    });

    AddTest("Default Constructor", [this](auto& ls)
    {
        Float3 tmp;

        if (tmp != Float3::Zero)
        {
            ls << "Default Float3 is not ZERO." << lferr;
        }
    });

    using namespace HSTL;

    AddTest("Constructors and Operators", [this](auto& ls)
    {
        HVector<Float3> vertices;

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

        Float3 tmp;

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

            ls << "Float3 Dot Time = " << Time::ToFloat(heTime)
                << ", Result = " << dotResult << lf;
        }
    });

    AddTest("Interpolation", [this](auto& ls)
    {
        auto x = Float3::X;
        auto y = Float3::Y;

        if (x != Float3::Slerp(x, y, 0.0f))
        {
            ls << "Float3 slerp 1 should match its origin " << x
                << ", but " << Float3::Slerp(x, y, 0.0f) << lferr;
        }

        if (y != Float3::Slerp(x, y, 1.0f))
        {
            ls << "Float3 slerp 1 should match its destination " << y
                << ", but " << Float3::Slerp(x, y, 1.0f) << lferr;
        }

        Float3 half = Float3::Slerp(x, y, 0.5f);
        ls << "Float3, Slerp half = " << half << lf;

        if (!IsEqual(half.Length(), 1.0f))
        {
            ls << "Float3 slerp of X and Y with 0.5f should have length 1 "
                << ", but " << half.Length() << lferr;
        }
    });
}

#endif //__UNIT_TEST__
