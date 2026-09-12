// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <iostream>
#include "Matrix3x3.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector3.h"

namespace hbe
{
	/// @brief A rigid body transform with rotation and translation but no scaling.
	template<typename TNumber = float, int PaddingSize = 8>
	class RigidTransform final
	{
		using This = RigidTransform;
		using TVec3 = Vector3<TNumber>;
		using TQuat = Quaternion<TNumber>;
		using TMat3x3 = Matrix3x3<TNumber>;
		using TMat4x4 = Matrix4x4<TNumber>;

	public:
		TQuat rotation;
		TVec3 translation;
		uint8_t padding[PaddingSize];

	public:
		RigidTransform() noexcept;
		explicit RigidTransform(std::nullptr_t) noexcept;
		RigidTransform(const TVec3& translation, const TQuat& rotation) noexcept;
		explicit RigidTransform(const TMat4x4& mat) noexcept;

		[[nodiscard]] bool operator==(const This& rhs) const noexcept
		{
			return rotation == rhs.rotation && translation == rhs.translation;
		}

		[[nodiscard]] bool operator!=(const This& rhs) const noexcept { return !(*this == rhs); }

		template<typename T>
		[[nodiscard]] T operator*(const T& rhs) const noexcept
		{
			return Transform(rhs);
		}

		[[nodiscard]] TVec3 Transform(const TVec3& x) const noexcept { return rotation * x + translation; }

		[[nodiscard]] TQuat Transform(const TQuat& r) const noexcept { return rotation * r; }

		[[nodiscard]] This Transform(const This& rhs) const noexcept
		{
			This result(nullptr);

			result.rotation = rotation * rhs.rotation;
			result.translation = rotation * rhs.translation + translation;

			return result;
		}

		[[nodiscard]] TVec3 InverseTransform(const TVec3& x) const noexcept
		{
			return (rotation.Inverse() * (x - translation));
		}

		[[nodiscard]] TQuat InverseTransform(const TQuat& r) const noexcept { return rotation.Inverse() * r; }

		[[nodiscard]] This InverseTransform(const This& rhs) const noexcept
		{
			This result(nullptr);

			result.rotation = InverseTransform(rhs.rotation);
			result.translation = InverseTransform(rhs.translation);

			return result;
		}

		[[nodiscard]] This Inverse() const noexcept
		{
			This inverse(nullptr);

			inverse.rotation = rotation.Inverse();
			inverse.translation = inverse.rotation * -translation;

			return inverse;
		}

		[[nodiscard]] TMat4x4 ToMatrix() const noexcept
		{
			TMat4x4 mat = rotation.ToMat4x4();
			mat.SetTranslation(translation);

			return mat;
		}
	};

	using TRigidTR = RigidTransform<float>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const RigidTransform<T>& local) noexcept
	{
		using namespace std;

		os << "Rigid Transform" << endl;
		os << "Position: (" << local.translation.x << ", " << local.translation.y << ", " << local.translation.z << ")"
		   << endl;
		auto r = local.rotation.EulerAngles();
		os << "Rotation: (" << r.x << ", " << r.y << ", " << r.z << ")" << endl;

		return os;
	}

	template<typename T, int N>
	RigidTransform<T, N>::RigidTransform() noexcept : rotation(), translation()
	{}

	template<typename T, int N>
	RigidTransform<T, N>::RigidTransform(std::nullptr_t) noexcept : rotation(nullptr), translation(nullptr)
	{}

	template<typename T, int N>
	RigidTransform<T, N>::RigidTransform(const TVec3& t, const TQuat& r) noexcept : rotation(r), translation(t)
	{}

	template<typename T, int N>
	RigidTransform<T, N>::RigidTransform(const TMat4x4& mat) noexcept :
		rotation(mat), translation(mat.m14, mat.m24, mat.m34)
	{
		Assert(mat.IsOrthogonal(), "RigidTransform::RigidTransform(Mat4x4) - matrix not orthogonal");
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class RigidTransformTest final : public TestCollection
	{
	public:
		RigidTransformTest() : TestCollection("RigidTransformTest") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
