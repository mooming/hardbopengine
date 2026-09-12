// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "AABB.h"
#include "Quaternion.h"
#include "Vector3.h"

namespace hbe
{
	/// @brief An Oriented Bounding Box class for collision detection.
	template<typename TNumber>
	class OBB final
	{
		using TVec3 = Vector3<TNumber>;
		using TQuat = Quaternion<TNumber>;

	public:
		// offset from an object space origin
		TVec3 center;
		// offset from the center
		TVec3 half;
		// rotation in an object space
		TQuat rotation;

	public:
		OBB() noexcept : center(), half(), rotation() {}

		explicit OBB(std::nullptr_t) noexcept : center(nullptr), half(nullptr), rotation(nullptr) {}

		OBB(const TVec3& center, const TVec3& half, const TQuat& rotation) noexcept : center(center), half(half), rotation(rotation)
		{}

		OBB(const AABB<TVec3>& aabb, const TQuat& rotation) noexcept :
			center((aabb.min + aabb.max) * 0.5f), half((aabb.max - aabb.min) * 0.5f), rotation(rotation)
		{}

		[[nodiscard]] bool IsContaining(const TVec3& objSpacePoint) const noexcept
		{
			auto point = ToOBBSpace(objSpacePoint);

			if (Abs(point.x) > half.x)
			{
				return false;
			}

			if (Abs(point.y) > half.y)
			{
				return false;
			}

			if (Abs(point.z) > half.z)
			{
				return false;
			}

			return true;
		}

		[[nodiscard]] TVec3 Closest(const TVec3& objSpacePoint) const noexcept
		{
			auto point = ToOBBSpace(objSpacePoint);

			auto length = TVec3::order;
			for (int i = 0; i < length; ++i)
			{
				float h = half.a[i];
				point.a[i] = ClampFast(point.a[i], -h, h);
			}

			return ToObjectSpace(point);
		}

		[[nodiscard]] bool HasIntersection(const OBB& obb, const TVec3& objPosition, const TQuat& objRot) noexcept { return false; }


	private:
		[[nodiscard]] TVec3 ToOBBSpace(const TVec3& objSpacePoint) const noexcept
		{
			auto point = objSpacePoint - center;
			point = rotation.Inverse() * point;

			return point;
		}

		[[nodiscard]] TVec3 ToObjectSpace(const TVec3& obbSpacePoint) const noexcept
		{
			auto point = rotation * obbSpacePoint;
			point -= center;

			return point;
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class OBBTest final : public TestCollection
	{
	public:
		OBBTest() : TestCollection("OBBTest") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
