// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Vector3.h"


namespace hbe
{
	template class Vector3<float>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Core/ScopedTime.h"
#include "HSTL/HVector.h"

void hbe::Vector3Test::Prepare() noexcept
{
	AddTest("Constants", [this](auto& ls)
	{
		ls << "Forward = " << TFloat3::Forward << lf;
		ls << "Right = " << TFloat3::Right << lf;
		ls << "Up = " << TFloat3::Up << lf;

		ls << "Zero = " << TFloat3::Zero << lf;
		ls << "Unity = " << TFloat3::Unity << lf;
	});

	AddTest("Cross Product", [this](auto& ls)
	{
		auto xCrossY = TFloat3::X.Cross(TFloat3::Y);
		ls << "X x Y = " << xCrossY << lf;

		if (xCrossY != TFloat3::Z)
		{
			ls << "Incorrect Cross Product Result: X x Y = " << xCrossY << ", but " << TFloat3::Z << " expected."
			   << lferr;
		}

#ifndef RIGHT_HANDED_COORDINATE
		AddTest("Left-Handed Coordinate Test", [this](auto& ls)
		{
			TFloat3 right = TFloat3::Right;
			TFloat3 up = TFloat3::Up;
			if (right.Cross(up) != TFloat3::Forward)
			{
				ls << "Cross Right x Up should be Forward..." << lferr;
			}
		});
#endif

#ifdef RIGHT_HANDED_COORDINATE
		AddTest("Right-Handed Coordinate Test", [this](auto& ls)
		{
			TFloat3 right = TFloat3::Right;
			TFloat3 up = TFloat3::Up;
			if (right.Cross(up) != -TFloat3::Forward)
			{
				ls << "Cross Right x Up should be -Forward..." << lferr;
			}
		});
#endif
	});

	AddTest("Default Constructor", [this](auto& ls)
	{
		TFloat3 tmp;

		if (tmp != TFloat3::Zero)
		{
			ls << "Default Float3 is not ZERO." << lferr;
		}
	});

	using namespace hbe;

	AddTest("Constructors and Operators", [this](auto& ls)
	{
		HVector<TFloat3> vertices;

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

		TFloat3 tmp;

		{
			time::TDuration heTime;
			float dotResult = 0.0f;

			{
				time::ScopedTime measure(heTime);

				for (auto& vertex : vertices)
				{
					dotResult += tmp.Dot(vertex);
					tmp = vertex;
				}
			}

			ls << "Float3 Dot Time = " << time::ToFloat(heTime) << ", Result = " << dotResult << lf;
		}
	});

	AddTest("Interpolation", [this](auto& ls)
	{
		auto x = TFloat3::X;
		auto y = TFloat3::Y;

		if (x != TFloat3::Slerp(x, y, 0.0f))
		{
			ls << "Float3 slerp 1 should match its origin " << x << ", but " << TFloat3::Slerp(x, y, 0.0f) << lferr;
		}

		if (y != TFloat3::Slerp(x, y, 1.0f))
		{
			ls << "Float3 slerp 1 should match its destination " << y << ", but " << TFloat3::Slerp(x, y, 1.0f) << lferr;
		}

		TFloat3 half = TFloat3::Slerp(x, y, 0.5f);
		ls << "Float3, Slerp half = " << half << lf;

		if (!IsEqual(half.Length(), 1.0f))
		{
			ls << "Float3 slerp of X and Y with 0.5f should have length 1 "
			   << ", but " << half.Length() << lferr;
		}
	});
}

#endif //__UNIT_TEST__
