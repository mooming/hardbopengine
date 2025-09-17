// Created by mooming.go@gmail.com

#include "Matrix3x3.h"

namespace hbe
{
	template class Matrix3x3<float>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "HSTL/HVector.h"

void hbe::Matrix3x3Test::Prepare()
{
	AddTest("Matrix3x3 Test", [this](auto& ls)
	{
		const auto& right = Float3::Right;
		const auto& up = Float3::Up;
		const auto& forward = Float3::Forward;

#ifdef __LEFT_HANDED__
		Float3x3 rRight = Float3x3::CreateRotation(90, 0, 0);
		Float3x3 rUp = Float3x3::CreateRotation(0, 90, 0);
		Float3x3 rForward = Float3x3::CreateRotation(0, 0, 90);
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
		Float3x3 rRight = Float3x3::CreateRotation(90, 0, 0);
		Float3x3 rUp = Float3x3::CreateRotation(0, 0, 90);
		Float3x3 rForward = Float3x3::CreateRotation(0, 90, 0);
#endif //__RIGHT_HANDED__

		if (rRight * right != right)
		{
			ls << "Rotation around right failed. " << (rRight * right) << ", but " << right << " expected." << lferr;
		}

		if (rUp * up != up)
		{
			ls << "Rotation around Y failed. " << (rUp * up) << ", but " << up << " expected." << lferr;
		}

		if (rForward * forward != forward)
		{
			ls << "Rotation around Z failed. " << (rForward * forward) << ", but " << forward << " expected." << lferr;
		}

		if (rRight * up != -forward)
		{
			ls << "Rotation around X failed. " << (rRight * up) << ", but " << forward << " expected." << lferr;
		}

		if (rUp * forward != -right)
		{
			ls << "Rotation around Y failed. " << (rUp * forward) << ", but " << right << " expected." << lferr;
		}

		if (rForward * right != -up)
		{
			ls << "Rotation around Z failed. " << (rForward * right) << ", but " << up << " expected." << lferr;
		}
	});
}

#endif //__UNIT_TEST__
