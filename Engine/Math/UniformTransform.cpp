// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "UniformTransform.h"


#ifdef __UNIT_TEST__
#include "Matrix4x4.h"

void hbe::UniformTransformTest::Prepare() noexcept
{
	AddTest("Constants", [this](auto& ls)
	{
		TUniformTRS tm;
		ls << tm << lf;

		if (tm != tm)
		{
			ls << "Self equality failed" << lferr;
		}

		if (tm.rotation != TQuat::Identity)
		{
			ls << "The default constructor of UniformTrs should have "
			   << "the identity rotation." << lferr;
		}

		if (tm.scale != 1.0f)
		{
			ls << "The default constructor of UniformTrs should have "
			   << "the identity scale." << lferr;
		}

		if (tm.translation != TFloat3::Zero)
		{
			ls << "The default constructor of UniformTrs should have "
			   << "the zero translation." << lferr;
		}

		TFloat4x4 mat = tm.ToMatrix();
		if (mat != TFloat4x4::Identity)
		{
			ls << "The default constructor of UniformTrs should be "
			   << "equal to the identity matrix." << std::endl
			   << mat << lferr;
		}

		if (tm * TFloat3::Forward != TFloat3::Forward)
		{
			ls << "Identity Transform Failed. " << (tm * TFloat3::Forward) << ", but " << TFloat3::Forward << " expected."
			   << lferr;
		}
	});

	AddTest("Inverse Matrix", [this](auto& ls)
	{
		TUniformTRS tm(TFloat3(4, 5, 6), TQuat(0, 47, 0), 2);
		TUniformTRS tm2(TFloat3(3, 4, 5), TQuat(41, 0, 25), 3);
		TFloat4x4 tm3 = (tm * tm2).ToMatrix();

		TFloat3 result1 = tm3 * TFloat4(TFloat3::Forward, 1.0f);
		TFloat3 result2 = tm * (tm2 * TFloat3::Forward);

		if (result1 != result2)
		{
			ls << "Transform result failed. " << (tm3 * TFloat4(TFloat3::Forward, 1.0f)) << ", but " << result2 << " expected."
			   << lferr;
		}

		if (!tm3.IsInvertible())
		{
			ls << "Transform matrix should be invertible." << std::endl << tm3 << lferr;
		}

		TFloat3 result = tm3.Inverse() * TFloat4(result1, 1.0f);
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
