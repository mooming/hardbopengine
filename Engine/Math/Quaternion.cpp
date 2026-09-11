// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Quaternion.h"


#ifdef __UNIT_TEST__
#include "Vector3.h"

namespace hbe
{

	void QuaternionTest::prepare() noexcept
	{
		static const TQuat x(90.0f, 0.0f, 0.0f);
		static const TQuat y(0.0f, 90.0f, 0.0f);
		static const TQuat z(0.0f, 0.0f, 90.0f);

		addTest("Constructors & Rotate Forward Vector", [&, this](auto& ls)
		{
			ls << "Quat rotate 90 around X = " << x << lf;
			ls << "Quat rotate 90 around Y = " << y << lf;
			ls << "Quat rotate 90 around Z = " << z << lf;

			ls << "Quat: Qx x Forward = " << x * TFloat3::Forward << lf;
			ls << "Quat: Qy x Forward = " << y * TFloat3::Forward << lf;
			ls << "Quat: Qz x Forward = " << z * TFloat3::Forward << lf;
		});

		addTest("90 degrees Rotation Test", [&](auto& ls)
		{
#ifndef RIGHT_HANDED_COORDINATE
			if (y * TFloat3::Right != TFloat3::Forward)
			{
				ls << "Quat rotation failed. " << (y * TFloat3::Right) << ", but " << TFloat3::Forward << " expected."
				   << lferr;
			}
#endif

#ifdef RIGHT_HANDED_COORDINATE
			if (y * TFloat3::Right != -TFloat3::Up)
			{
				ls << "Quat rotation failed. " << (y * TFloat3::Right) << ", but " << -TFloat3::Up << " expected."
				   << lferr;
			}
#endif
		});

		addTest("RotationFromTo", [&, this](auto& ls)
		{
			TQuat xToY(nullptr);
			xToY.setRotationFromTo(TFloat3::X, TFloat3::Y);

			if (xToY * TFloat3::X != TFloat3::Y)
			{
				ls << "Quat from-to rotation failed. " << (xToY * TFloat3::X) << ", but " << TFloat3::Y << " expected."
				   << lferr;
			}
		});

		addTest("Composition", [&, this](auto& ls)
		{
			TQuat yx(90.0f, 90.0f, 0.0f);

			ls << "Quat YX = " << yx << lf;
			ls << "Quat X x Y = " << (x * y) << lf;
			ls << "Quat Y x X = " << (y * x) << lf;

			if (yx != (y * x))
			{
				ls << "Quat: yx is not equal to (y * x)." << lferr;
			}

			TQuat zyx(90.0f, 90.0f, 90.0f);
			ls << "Quat ZYX = " << zyx << lf;
			ls << "Quat X x Y x Z = " << (x * y * z) << lf;
			ls << "Quat Z x Y x X = " << (z * y * x) << lf;

			if (zyx != (z * y * x))
			{
				ls << "Quat: zyx = " << zyx << " is not equal to (z * y * x) = " << (z * y * x) << lferr;
			}
		});

		addTest("Cast to Rotation Matrix", [&, this](auto& ls)
		{
			TQuat zyx(90.0f, 90.0f, 90.0f);
			TFloat3x3 matZyx = zyx;

			if ((matZyx * TFloat3::Forward) != (zyx * TFloat3::Forward))
			{
				ls << "Quat: matrix representation is not coincident." << lferr;
			}
		});

		addTest("Create rotations", [&, this](auto& ls)
		{
			if (TQuat() != TQuat::createRotationX(0.0f))
			{
				ls << "Quat: CreateRotationX(0) failed." << lferr;
			}

			if (TQuat() != TQuat::createRotationY(0.0f))
			{
				ls << "Quat: CreateRotationY(0) failed." << lferr;
			}

			if (TQuat() != TQuat::createRotationXY(0.0f, 0.0f))
			{
				ls << "Quat: CreateRotationXY(0) failed." << lferr;
			}

			if (TQuat() != TQuat::createRotationYZ(0.0f, 0.0f))
			{
				ls << "Quat: CreateRotationYZ(0) failed." << lferr;
			}

			if (TQuat() != TQuat::createRotationXZ(0.0f, 0.0f))
			{
				ls << "Quat: CreateRotationXZ(0) failed." << lferr;
			}

			if (TQuat() != TQuat(0.0f, 0.0f, 0.0f))
			{
				ls << "Quat(0, 0, 0) failed." << lferr;
			}
		});

		addTest("Look Rotation", [&, this](auto& ls)
		{
			TQuat look = TQuat::lookRotation(TFloat3::Forward, TFloat3::Up);

			ls << "Look = " << look << lf;

			if (look != TQuat())
			{
				ls << "Quat: LookRotation failed." << lferr;
			}

			TQuat lookBack = TQuat::lookRotation(-TFloat3::Forward, TFloat3::Up);
			ls << "Look Backward = " << lookBack << lf;

			if ((lookBack * TFloat3::Forward) != (-TFloat3::Forward))
			{
				ls << "Quat: LookRotation, look back failed." << lferr;
			}
		});

		addTest("Rotation Matrix Comparison", [&, this](auto& ls)
		{
			if ((y * TFloat3::Forward) != (TFloat3x3(y) * TFloat3::Forward))
			{
				ls << "Quat: Matrix rotation is not coincided. " << (y * TFloat3::Forward)
				   << " != " << (TFloat3x3(y) * TFloat3::Forward) << lferr;
			}
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
