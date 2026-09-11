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

	void TransformTest::prepare() noexcept
	{
		addTest("Default Constructor", [this](auto& ls)
		{
			TFTransform root;
			ls << root << lf;
		});

		addTest("Hierachycal Transform", [this](auto& ls)
		{
			TFTransform root;
			TFTransform a;
			root.attach(a);

			TFTransform b;
			a.attach(b);

			TFTransform c;
			b.attach(c);

			a.set(TFloat3(0, 0, 1));
			b.set(TFloat3(0, 1, 0));
			c.set(TFloat3(1, 0, 0));

			ls << "a = " << a << lf;
			ls << "b = " << b << lf;
			ls << "c = " << c << lf;

			auto worldC = c.getWorldTransform().translation;
			if (worldC != TFloat3(1, 1, 1))
			{
				ls << "c(" << worldC << ") doesn't coincide with (1, 1, 1)" << lferr;
			}
		});

		addTest("Hierachycal Rotation", [this](auto& ls)
		{
			TFTransform root;
			TFTransform a;
			root.attach(a);

			TFTransform b;
			a.attach(b);

			b.set(TFloat3(0, 0, 1));
			a.set(TQuat(0.0f, 45.0f, 0.0f));

			ls << "b = " << b << lf;

			auto worldB = b.getWorldTransform().translation;
			if (!worldB.isUnity())
			{
				ls << "b " << worldB << " is not the unity." << lferr;
			}

			a.set(TQuat(0.0f, 90.0f, 0.0f));

			ls << "b = " << b << lf;

			worldB = b.getWorldTransform().translation;
			if (worldB != TFloat3(1, 0, 0))
			{
				ls << "b " << worldB << " doesn't coincide with (1, 0, 0)" << lferr;
			}
		});

		addTest("Hierachycal Rotation (2)", [this](auto& ls)
		{
			TFTransform root;
			TFTransform a;
			root.attach(a);

			TFTransform b;
			a.attach(b);

			TFTransform c;
			b.attach(c);

			a.set(TQuat(0.0f, 45.0f, 0.0f));
			b.set(TQuat(0.0f, 45.0f, 0.0f));
			c.set(TFloat3(0, 0, 1));

			ls << "c = " << c << lf;

			auto worldC = c.getWorldTransform().translation;
			if (!worldC.isUnity())
			{
				ls << "The world transformed c" << worldC << " is not unity." << lferr;
			}

			ls << "c = " << c << lf;

			worldC = c.getWorldTransform().translation;
			if (c.getWorldTransform().translation != TFloat3(1, 0, 0))
			{
				ls << "The world transformed c" << worldC << " doesn't coincide with (1, 0, 0)" << lferr;
			}
		});
	}

} // namespace hbe

#endif //__UNIT_TEST__
