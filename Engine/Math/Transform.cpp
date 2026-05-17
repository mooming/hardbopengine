// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Transform.h"


namespace hbe
{
	template class Transform<float>;
	template class Transform<double>;
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Core/CommonUtil.h"
#include "Vector3.h"

namespace hbe
{

	void TransformTest::Prepare() noexcept
	{
		AddTest("Default Constructor", [this](auto& ls)
		{
			TFTransform root;
			ls << root << lf;
		});

		AddTest("Hierachycal Transform", [this](auto& ls)
		{
			TFTransform root;
			TFTransform a;
			root.Attach(a);

			TFTransform b;
			a.Attach(b);

			TFTransform c;
			b.Attach(c);

			a.Set(TFloat3(0, 0, 1));
			b.Set(TFloat3(0, 1, 0));
			c.Set(TFloat3(1, 0, 0));

			ls << "a = " << a << lf;
			ls << "b = " << b << lf;
			ls << "c = " << c << lf;

			auto worldC = c.GetWorldTransform().translation;
			if (worldC != TFloat3(1, 1, 1))
			{
				ls << "c(" << worldC << ") doesn't coincide with (1, 1, 1)" << lferr;
			}
		});

		AddTest("Hierachycal Rotation", [this](auto& ls)
		{
			TFTransform root;
			TFTransform a;
			root.Attach(a);

			TFTransform b;
			a.Attach(b);

			b.Set(TFloat3(0, 0, 1));
			a.Set(TQuat(0.0f, 45.0f, 0.0f));

			ls << "b = " << b << lf;

			auto worldB = b.GetWorldTransform().translation;
			if (!worldB.IsUnity())
			{
				ls << "b " << worldB << " is not the unity." << lferr;
			}

			a.Set(TQuat(0.0f, 90.0f, 0.0f));

			ls << "b = " << b << lf;

			worldB = b.GetWorldTransform().translation;
			if (worldB != TFloat3(1, 0, 0))
			{
				ls << "b " << worldB << " doesn't coincide with (1, 0, 0)" << lferr;
			}
		});

		AddTest("Hierachycal Rotation (2)", [this](auto& ls)
		{
			TFTransform root;
			TFTransform a;
			root.Attach(a);

			TFTransform b;
			a.Attach(b);

			TFTransform c;
			b.Attach(c);

			a.Set(TQuat(0.0f, 45.0f, 0.0f));
			b.Set(TQuat(0.0f, 45.0f, 0.0f));
			c.Set(TFloat3(0, 0, 1));

			ls << "c = " << c << lf;

			auto worldC = c.GetWorldTransform().translation;
			if (!worldC.IsUnity())
			{
				ls << "The world transformed c" << worldC << " is not unity." << lferr;
			}

			ls << "c = " << c << lf;

			worldC = c.GetWorldTransform().translation;
			if (c.GetWorldTransform().translation != TFloat3(1, 0, 0))
			{
				ls << "The world transformed c" << worldC << " doesn't coincide with (1, 0, 0)" << lferr;
			}
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
