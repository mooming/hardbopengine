#pragma once

#include "AABB.h"
#include "Vector3.h"
#include "Quaternion.h"

namespace HE
{
	template <typename Number>
	class OBB
	{
		using Vec3 = Vector3<Number>;
		using Quat = Quaternion<Number>;

	public:
		// offset from an object space origin
		Vec3 center;
		// offset from the center
		Vec3 half;
		// rotation in an object space
		Quat rotation;

	public:
		OBB() : center(), half(), rotation()
		{
		}

		OBB(std::nullptr_t) : center(nullptr), half(nullptr), rotation(nullptr)
		{
		}

		OBB(const Vec3& center, const Vec3& half, const Quat& rotation)
			: center(center), half(half), rotation(rotation)
		{
		}

		OBB(const AABB<Vec3>& aabb, const Quat& rotation)
			: center((aabb.min + aabb.max) * 0.5f)
			, half((aabb.max - aabb.min) * 0.5f)
			, rotation(rotation)
		{
		}

		bool IsContaining(const Vec3& objSpacePoint) const
		{
			auto point = ToOBBSpace(objSpacePoint);

			if (Abs(point.x) > half.x)
				return false;

			if (Abs(point.y) > half.y)
				return false;

			if (Abs(point.z) > half.z)
				return false;

			return true;
		}

		Vec3 Closest(const Vec3& objSpacePoint) const
		{
			auto point = ToOBBSpace(objSpacePoint);

			auto length = Vec3::order;
			for (int i = 0; i < length; ++i)
			{
				float h = half.a[i];
				point.a[i] = ClampFast(point.a[i], -h, h);
			}

			return ToObjectSpace(point);
		}

		bool HasIntersection(const OBB& obb, const Vec3& objPosition, const Quat& objRot)
		{

			return false;
		}

	private:
		Vec3 ToOBBSpace(const Vec3& objSpacePoint) const
		{
			auto point = objSpacePoint - center;
			point = rotation.Inverse() * point;

			return point;
		}

		Vec3 ToObjectSpace(const Vec3& obbSpacePoint) const
		{
			auto point = rotation * obbSpacePoint;
			point -= center;

			return point;
		}
	};
}

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{

	class OBBTest : public TestCase
	{
	public:

		OBBTest() : TestCase("OBBTest")
		{
		}

	protected:
		virtual bool DoTest() override;
	};
}
#endif //__UNIT_TEST__

