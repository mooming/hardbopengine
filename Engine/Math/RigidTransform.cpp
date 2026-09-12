// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RigidTransform.h"


using namespace hbe;

#ifdef __UNIT_TEST__
#include "Matrix4x4.h"

void RigidTransformTest::Prepare() noexcept
{
	AddTest("Default RigidTransform & Constants", [this](auto& ls)
	{
		TRigidTR tm;
		ls << tm << lf;

		if (tm != tm)
		{
			ls << "Self equality failed" << lferr;
		}

		if (tm.rotation != TQuat::Identity)
		{
			ls << "The default constructor of RigidTR should have "
			   << "the identity rotation." << lferr;
		}

		if (tm.translation != TFloat3::Zero)
		{
			ls << "The default constructor of RigidTR should have "
			   << "the zero translation." << lferr;
		}

		TFloat4x4 mat = tm.ToMatrix();
		if (mat != TFloat4x4::Identity)
		{
			ls << "The default constructor of RigidTR should be "
			   << "equal to the identity matrix." << lferr;
		}

		if (tm * TFloat3::Forward != TFloat3::Forward)
		{
			ls << "Identity Transform Failed. " << (tm * TFloat3::Forward) << ", but " << TFloat3::Forward << " expected."
			   << lferr;
		}
	});

	AddTest("RigidTransform Operations", [this](auto& ls)
	{
		TRigidTR tm(TFloat3(4, 5, 6), TQuat(0, 47, 0));
		TRigidTR tm2(TFloat3(3, 4, 5), TQuat(41, 0, 25));
		TRigidTR tm3 = tm * tm2;

		ls << tm << lf;
		ls << tm2 << lf;
		ls << tm3 << lf;

		TFloat3 result1 = tm3 * TFloat3::Forward;
		TFloat3 result2 = tm * (tm2 * TFloat3::Forward);
		if (result1 != result2)
		{
			ls << "Transform result failed. " << (tm3 * TFloat3::Forward) << ", but " << result2 << " expected."
			   << lferr;
		}

		if (!tm3.ToMatrix().IsInvertible())
		{
			ls << "Transform matrix should be invertible." << tm3 << lferr;
		}

		TFloat3 result = tm3.Inverse() * result1;
		if (result != TFloat3::Forward)
		{
			ls << "Transform matrix inverse failed. " << result << ", but " << TFloat3::Forward << " expected." << lferr;
		}

		result = tm2.InverseTransform(tm.InverseTransform(result2));
		if (result != TFloat3::Forward)
		{
			ls << "Inverse transform failed. " << result << ", but " << TFloat3::Forward << " expected." << lferr;
		}

		if (tm3.Inverse().Inverse() != tm3)
		{
			ls << "Inverse of inverse transform failed. " << tm3.Inverse().Inverse() << ", but " << tm3 << " expected."
			   << lferr;
		}
	});
}

#endif //__UNIT_TEST__
