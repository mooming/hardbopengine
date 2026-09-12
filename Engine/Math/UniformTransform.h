// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <ostream>
#include "Log/Logger.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector3.h"

namespace hbe
{
	/// @brief A transform with uniform scale, rotation, and translation.
	template<typename TNumber>
	class UniformTransform final
	{
		using This = UniformTransform;
		using TVec3 = Vector3<TNumber>;
		using TQuat = Quaternion<TNumber>;
		using TMat3x3 = Matrix3x3<TNumber>;
		using TMat4x4 = Matrix4x4<TNumber>;

	public:
		TQuat rotation;
		TNumber scale;
		TVec3 translation;

	public:
		UniformTransform() noexcept : scale(1) {}

		explicit UniformTransform(std::nullptr_t) noexcept : rotation(nullptr), translation(nullptr) {}

		UniformTransform(const TVec3& translation, const TQuat& rotation, TNumber scale) noexcept :
			rotation(rotation), scale(scale), translation(translation)
		{}

		explicit UniformTransform(const TMat4x4& mat) noexcept
		{
			Assert(mat.IsOrthogonal(), "UniformTransform::UniformTransform(Mat4x4) - matrix not orthogonal");

			TVec3 c1 = TVec3(mat.m11, mat.m21, mat.m31);
			TVec3 c2 = TVec3(mat.m12, mat.m22, mat.m32);
			TVec3 c3 = TVec3(mat.m13, mat.m23, mat.m33);

			constexpr float oneThird = 1.0f / 3.0f;
			scale = (c1.Normalize() + c2.Normalize() + c3.Normalize()) * oneThird;

			Assert(IsEqual(scale, c2.Normalize()), "UniformTransform - scale mismatch");
			Assert(IsEqual(scale, c3.Normalize()), "UniformTransform - scale mismatch");

			TMat3x3 rotMat(nullptr);

			rotMat.m11 = c1.x;
			rotMat.m21 = c1.y;
			rotMat.m31 = c1.z;

			rotMat.m12 = c2.x;
			rotMat.m22 = c2.y;
			rotMat.m32 = c2.z;

			rotMat.m13 = c3.x;
			rotMat.m23 = c3.y;
			rotMat.m33 = c3.z;

			rotation = static_cast<TQuat>(rotMat);
		}

		[[nodiscard]] bool operator==(const This& rhs) const noexcept
		{
			return rotation == rhs.rotation && scale == rhs.scale && translation == rhs.translation;
		}

		[[nodiscard]] bool operator!=(const This& rhs) const noexcept { return !(*this == rhs); }

		template<typename T>
		[[nodiscard]] T operator*(const T& rhs) const noexcept
		{
			return Transform(rhs);
		}

		[[nodiscard]] TVec3 Transform(const TVec3& x) const noexcept { return rotation * (scale * x) + translation; }

		[[nodiscard]] TVec3 InverseTransform(const TVec3& x) const noexcept
		{
			return (rotation.Inverse() * (x - translation) / scale);
		}

		[[nodiscard]] This Transform(const This& rhs) const noexcept
		{
			This result(nullptr);

			result.scale = scale * rhs.scale;
			result.rotation = rotation * rhs.rotation;
			result.translation = rotation * (scale * rhs.translation) + translation;

			return result;
		}

		[[nodiscard]] This Inverse() const noexcept
		{
			This inverse(nullptr);

			scale = 1.0f / scale;
			rotation.Invert();
			translation = rotation * translation * (-scale);

			return inverse;
		}

		[[nodiscard]] TMat4x4 ToMatrix() const noexcept
		{
			TMat4x4 mat = rotation.ToMat4x4() * TMat4x4::CreateDiagonal(TFloat4(scale, scale, scale, 1.0f));
			mat.SetTranslation(translation);

			return mat;
		}
	};

	using TUniformTRS = UniformTransform<float>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const UniformTransform<T>& local) noexcept
	{
		using namespace std;

		os << "Uniform Transform" << endl;

		os << "Position: (" << local.translation.x << ", " << local.translation.y << ", " << local.translation.z << ")"
		   << endl;

		auto r = local.rotation.EulerAngles();
		os << "Rotation: (" << r.x << ", " << r.y << ", " << r.z << ")" << endl;
		os << "Scale: (" << local.scale << ")" << endl;

		return os;
	}

	template<typename T>
	LogStream& operator<<(LogStream& os, const UniformTransform<T>& local) noexcept
	{
		os << "Uniform Transform" << hendl;

		os << "Position: (" << local.translation.x << ", " << local.translation.y << ", " << local.translation.z << ")"
		   << hendl;

		auto r = local.rotation.EulerAngles();
		os << "Rotation: (" << r.x << ", " << r.y << ", " << r.z << ")" << hendl;
		os << "Scale: (" << local.scale << ")" << hendl;

		return os;
	}
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class UniformTransformTest final : public TestCollection
	{
	public:
		UniformTransformTest() : TestCollection("UniformTransformTest") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
