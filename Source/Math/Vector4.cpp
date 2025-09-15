// Created by mooming.go@gmail.com

#include "Vector4.h"

namespace hbe
{
	template class Vector4<float>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Core/ScopedTime.h"
#include "HSTL/HVector.h"

void hbe::Vector4Test::Prepare()
{
	AddTest("Vector4 Constants", [this](auto& ls)
	{
		ls << "Forward = " << Float4::Forward << lf;
		ls << "Right = " << Float4::Right << lf;
		ls << "Up = " << Float4::Up << lf;

		ls << "Zero = " << Float4::Zero << lf;
		ls << "Unity = " << Float4::Unity << lf;

		Float4 tmp;
		if (tmp != Float4::Zero)
		{
			ls << "Default Float4 is not ZERO. Default = " << tmp << lferr;
		}
	});

	AddTest("Vector4 Constructors & Operator", [this](auto& ls)
	{
		HSTL::HVector<Float4> vertices;

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

		Float4 tmp;

		Time::TDuration heTime;
		float dotResult = 0.0f;

		{
			Time::ScopedTime measure(heTime);

			for (auto& vertex : vertices)
			{
				dotResult += tmp.Dot(vertex);
				tmp = vertex;
			}
		}

		ls << "Float4 Dot Time = " << Time::ToFloat(heTime) << ", Result = " << dotResult << lf;
	});
}

#endif //__UNIT_TEST__
