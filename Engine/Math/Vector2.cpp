// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Vector2.h"


namespace hbe
{
	template class Vector2<float>;
}

#ifdef __UNIT_TEST__

void hbe::Vector2Test::Prepare() noexcept
{
	AddTest("Vector2 Constants & Operations", [this](auto& ls)
	{
		ls << "Zero = " << TFloat2::Zero << lf;
		ls << "X = " << TFloat2::X << lf;
		ls << "Y = " << TFloat2::Y << lf;

		if ((TFloat2::X + TFloat2::Y) != TFloat2(1.0f, 1.0f))
		{
			ls << "Float2 addition failed. X + Y = " << (TFloat2::X + TFloat2::Y) << lferr;
		}

		if (!TFloat2::Zero.IsZero())
		{
			ls << "Float2 zero test failed. zero = " << TFloat2::Zero << lferr;
		}
	});
}

#endif //__UNIT_TEST__
