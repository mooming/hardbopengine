// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "Quaternion.h"


#ifdef __UNIT_TEST__
#include "Vector3.h"

namespace HE
{

void QuaternionTest::Prepare()
{
    static const Quat x(90.0f, 0.0f, 0.0f);
    static const Quat y(0.0f, 90.0f, 0.0f);
    static const Quat z(0.0f, 0.0f, 90.0f);

    AddTest("Constructors & Rotate Forward Vector", [&, this](auto& ls)
    {
        ls << "Quat rotate 90 around X = " << x << lf;
        ls << "Quat rotate 90 around Y = " << y << lf;
        ls << "Quat rotate 90 around Z = " << z << lf;

        ls << "Quat: Qx x Forward = " << x * Float3::Forward << lf;
        ls << "Quat: Qy x Forward = " << y * Float3::Forward << lf;
        ls << "Quat: Qz x Forward = " << z * Float3::Forward << lf;
    });

    AddTest("90 degrees Rotation Test", [&, this](auto& ls)
    {
#ifdef __LEFT_HANDED__
        if (y * Float3::Right != Float3::Forward)
        {
            ls << "Quat rotation failed. " << (y * Float3::Right) << ", but "
                << Float3::Forward << " expected." << lferr;
        }
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
        if (z * Float3::Right != Float3::Forward)
        {
            ls << "Quat rotation failed. " << (z * Float3::Right) << ", but "
                << Float3::Forward << " expected." << lferr;
        }
#endif //__RIGHT_HANDED__
    });

    AddTest("RotationFromTo", [&, this](auto& ls)
    {
        Quat xToY(nullptr);
        xToY.SetRotationFromTo(Float3::X, Float3::Y);

        if (xToY * Float3::X != Float3::Y)
        {
            ls << "Quat from-to rotation failed. " << (xToY * Float3::X)
                << ", but " << Float3::Y << " expected." << lferr;
        }
    });

    AddTest("Composition", [&, this](auto& ls)
    {
        Quat yx(90.0f, 90.0f, 0.0f);

        ls << "Quat YX = " << yx << lf;
        ls << "Quat X x Y = " << (x * y) << lf;
        ls << "Quat Y x X = " << (y * x) << lf;

        if (yx != (y * x))
        {
            ls << "Quat: yx is not equal to (y * x)." << lferr;
        }

        Quat zyx(90.0f, 90.0f, 90.0f);
        ls << "Quat ZYX = " << zyx << lf;
        ls << "Quat X x Y x Z = " << (x * y * z) << lf;
        ls << "Quat Z x Y x X = " << (z * y * x) << lf;

        if (zyx != (z * y * x))
        {
            ls << "Quat: zyx = " << zyx << " is not equal to (z * y * x) = "
                << (z * y * x) << lferr;
        }
    });

    AddTest("Cast to Rotation Matrix", [&, this](auto& ls)
    {
        Quat zyx(90.0f, 90.0f, 90.0f);
        Float3x3 matZyx = zyx;

        if ((matZyx * Float3::Forward) != (zyx * Float3::Forward))
        {
            ls << "Quat: matrix representation is not coincident." << lferr;
        }
    });


    AddTest("Create rotations", [&, this](auto& ls)
    {
        if (Quat() != Quat::CreateRotationX(0.0f))
        {
            ls << "Quat: CreateRotationX(0) failed." << lferr;
        }

        if (Quat() != Quat::CreateRotationY(0.0f))
        {
            ls << "Quat: CreateRotationY(0) failed." << lferr;
        }

        if (Quat() != Quat::CreateRotationXY(0.0f, 0.0f))
        {
            ls << "Quat: CreateRotationXY(0) failed." << lferr;
        }

        if (Quat() != Quat::CreateRotationYZ(0.0f, 0.0f))
        {
            ls << "Quat: CreateRotationYZ(0) failed." << lferr;
        }

        if (Quat() != Quat::CreateRotationXZ(0.0f, 0.0f))
        {
            ls << "Quat: CreateRotationXZ(0) failed." << lferr;
        }

        if (Quat() != Quat(0.0f, 0.0f, 0.0f))
        {
            ls << "Quat(0, 0, 0) failed." << lferr;
        }
    });

    AddTest("Look Rotation", [&, this](auto& ls)
    {
        Quat look = Quat::LookRotation(Float3::Forward, Float3::Up);

        ls << "Look = " << look << lf;

        if (look != Quat())
        {
            ls << "Quat: LookRotation failed." << lferr;
        }

        Quat lookBack = Quat::LookRotation(-Float3::Forward, Float3::Up);
        ls << "Look Backward = " << lookBack << lf;

        if ((lookBack * Float3::Forward) != (-Float3::Forward))
        {
            ls << "Quat: LookRotation, look back failed." << lferr;
        }
    });


    AddTest("Rotation Matrix Comparison", [&, this](auto& ls)
    {
        if ((y * Float3::Forward) != (Float3x3(y) * Float3::Forward))
        {
            ls << "Quat: Matrix rotation is not coincided. "
                << (y * Float3::Forward) << " != "
                << (Float3x3(y) * Float3::Forward) << lferr;
        }
    });
}

} //HE

#endif //__UNIT_TEST__
