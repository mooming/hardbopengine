// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <iostream>
#include "CoordinateOrientation.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

namespace hbe
{
	/// @brief A quaternion template class for representing 3D rotations.
	// Assume every quaternion is a unit quaternion.

	template<typename TNumber>
	class Quaternion final
	{
		using This = Quaternion;
		using TVec3 = Vector3<TNumber>;
		using TVec4 = Vector4<TNumber>;
		using TMat3x3 = Matrix3x3<TNumber>;
		using TMat4x4 = Matrix4x4<TNumber>;

	public:
		static const This Zero;
		static const This Identity;

	public:
		union
		{
			struct
			{
				TNumber x;
				TNumber y;
				TNumber z;
				TNumber w;
			};

			struct
			{
				TVec3 v;
				TNumber s;
			};

			TVec4 vector;
			TNumber a[4];
		};

	public:
		[[nodiscard]] static Quaternion CreateRotationX(const float x) noexcept
		{
			Quaternion result(nullptr);
			result.SetEulerX(x);

			return result;
		}

		[[nodiscard]] static Quaternion CreateRotationY(const float y) noexcept
		{
			Quaternion result(nullptr);
			result.SetEulerY(y);

			return result;
		}

		[[nodiscard]] static Quaternion CreateRotationZ(const float z) noexcept
		{
			Quaternion result(nullptr);
			result.SetEulerZ(z);

			return result;
		}

		[[nodiscard]] static Quaternion CreateRotationXY(float x, float y) noexcept
		{
			Quaternion result(nullptr);
			result.SetEulerXY(x, y);

			return result;
		}

		[[nodiscard]] static Quaternion CreateRotationYZ(float y, float z) noexcept
		{
			Quaternion result(nullptr);
			result.SetEulerYZ(y, z);

			return result;
		}

		[[nodiscard]] static Quaternion CreateRotationXZ(float x, float z) noexcept
		{
			Quaternion result(nullptr);
			result.SetEulerXZ(x, z);

			return result;
		}

		[[nodiscard]] static Quaternion LookRotation(const TVec3& forward, const TVec3& up) noexcept
		{
			Quaternion result(nullptr);
			result.LookAt(forward, up);

			return result;
		}

		[[nodiscard]] static Quaternion RotationAround(const TVec3& unitAxis, float radian) noexcept
		{
			Quaternion result(nullptr);
			result.SetRotationAround(unitAxis, radian);

			return result;
		}

		[[nodiscard]] static Quaternion RotationFromTo(const TVec3& from, const TVec3& to) noexcept
		{
			Quaternion result(nullptr);
			result.SetRotationFromTo(from, to);

			return result;
		}

		Quaternion() noexcept : v(), s(1.0f) {}

		Quaternion(const TVec3& eulerAngles) noexcept : Quaternion(nullptr)
		{
			SetEulerAngles(eulerAngles.x, eulerAngles.y, eulerAngles.z);
		}

		Quaternion(float x, float y, float z) noexcept : Quaternion(nullptr) { SetEulerAngles(x, y, z); }

		Quaternion(float x, float y, float z, float w) noexcept : vector(x, y, z, w) {}

		Quaternion(const TVec4& vector) noexcept : vector(vector) {}

		Quaternion(const TMat3x3& mat) noexcept
		{
			Assert(mat.IsOrthogonal(), "[Quaternion] Given matrix is not orthogonal!", mat);
			Assert(IsEqual(mat.Determinant(), 1.0f), "[Quaternion] Matrix Determinant should be 1, but ",
				   mat.Determinant());

			const float trace = mat.m11 + mat.m22 + mat.m33;
			if (trace > -1.0f)
			{
				const float tmp = sqrtf(1.0f + trace) * 0.5f;
				const float reciprocal = 0.25f / tmp;

				x = (mat.m32 - mat.m23) * reciprocal;
				y = (mat.m13 - mat.m31) * reciprocal;
				z = (mat.m21 - mat.m12) * reciprocal;
				w = tmp;
			}
			else if (mat.m11 > mat.m22 && mat.m11 > mat.m33)
			{
				x = sqrt(1.0f + mat.m11 - mat.m22 - mat.m33) * 0.5f;
				const float reciprocal = 0.25f / x;
				y = (mat.m12 + mat.m21) * reciprocal;
				z = (mat.m13 + mat.m31) * reciprocal;
				w = (mat.m32 - mat.m23) * reciprocal;
			}
			else if (mat.m22 > mat.m33)
			{
				const float tmp = sqrt(1.0f - mat.m11 + mat.m22 - mat.m33) * 0.5f;
				const float reciprocal = 0.25f / tmp;
				x = (mat.m12 + mat.m21) * reciprocal;
				y = tmp;
				z = (mat.m23 + mat.m32) * reciprocal;
				w = (mat.m13 - mat.m31) * reciprocal;
			}
			else
			{
				const float tmp = sqrt(1.0f - mat.m11 - mat.m22 + mat.m33) * 0.5f;
				const float reciprocal = 0.25f / tmp;

				x = (mat.m13 + mat.m31) * reciprocal;
				y = (mat.m23 + mat.m32) * reciprocal;
				z = tmp;
				w = (mat.m21 - mat.m12) * reciprocal;
			}

			Normalize();
		}

		explicit Quaternion(std::nullptr_t) noexcept {}

		[[nodiscard]] TMat3x3 ToMat3x3() const noexcept
		{
			TMat3x3 mat(nullptr);

			const float xx = x * x;
			const float yy = y * y;
			const float zz = z * z;

			const float xy = x * y;
			const float yz = y * z;
			const float xz = x * z;

			const float xw = x * w;
			const float yw = y * w;
			const float zw = z * w;

			mat.m11 = 1.0f - 2.0f * (yy + zz);
			mat.m12 = 2.0f * (xy - zw);
			mat.m13 = 2.0f * (xz + yw);

			mat.m21 = 2.0f * (xy + zw);
			mat.m22 = 1.0f - 2.0f * (xx + zz);
			mat.m23 = 2.0f * (yz - xw);

			mat.m31 = 2.0f * (xz - yw);
			mat.m32 = 2.0f * (yz + xw);
			mat.m33 = 1.0f - 2.0f * (xx + yy);

			return mat;
		}

		[[nodiscard]] TMat4x4 ToMat4x4() const noexcept
		{
			TMat4x4 mat(nullptr);

			const float xx = x * x;
			const float yy = y * y;
			const float zz = z * z;

			const float xy = x * y;
			const float yz = y * z;
			const float xz = x * z;

			const float xw = x * w;
			const float yw = y * w;
			const float zw = z * w;

			mat.m11 = 1.0f - 2.0f * (yy + zz);
			mat.m12 = 2.0f * (xy - zw);
			mat.m13 = 2.0f * (xz + yw);
			mat.m14 = 0.0f;

			mat.m21 = 2.0f * (xy + zw);
			mat.m22 = 1.0f - 2.0f * (xx + zz);
			mat.m23 = 2.0f * (yz - xw);
			mat.m24 = 0.0f;

			mat.m31 = 2.0f * (xz - yw);
			mat.m32 = 2.0f * (yz + xw);
			mat.m33 = 1.0f - 2.0f * (xx + yy);
			mat.m34 = 0.0f;

			mat.m41 = 0.0f;
			mat.m42 = 0.0f;
			mat.m43 = 0.0f;
			mat.m44 = 1.0f;

			return mat;
		}

		operator TMat3x3() const noexcept { return ToMat3x3(); }

		operator TMat4x4() const noexcept { return ToMat4x4(); }

		[[nodiscard]] TVec3 operator*(const TVec3& rhs) const noexcept { return Multiply(rhs); }

		[[nodiscard]] Quaternion operator*(const Quaternion& rhs) const noexcept { return Multiply(rhs); }

		[[nodiscard]] Quaternion operator/(const Quaternion& rhs) const noexcept { return Multiply(rhs.Inverse()); }

		Quaternion& operator*=(const Quaternion& rhs) noexcept
		{
			*this = Multiply(rhs);

			return *this;
		}

		Quaternion& operator/=(const Quaternion& rhs) noexcept
		{
			*this = Multiply(rhs.Inverse());

			return *this;
		}

		[[nodiscard]] bool operator==(const Quaternion& rhs) const noexcept { return vector == rhs.vector; }

		[[nodiscard]] bool operator!=(const Quaternion& rhs) const noexcept { return vector != rhs.vector; }

		void Conjugate() noexcept { v.Negate(); }

		[[nodiscard]] Quaternion Conjugated() const noexcept
		{
			Quaternion result(*this);
			result.Conjugate();

			return result;
		}

		// Invert itself

		void Invert() noexcept
		{
			Assert(!vector.IsZero(), "Quaternion::Invert - vector is zero");
			Assert(IsEqual(vector.SqrLength(), 1.0f), "Quaternion::Invert - not unit length");

			Conjugate();
		}

		// Return the inverse of it

		[[nodiscard]] Quaternion Inverse() const noexcept
		{
			Quaternion result(*this);
			result.Invert();

			return result;
		}

		void Normalize() noexcept { vector.Normalize(); }

		[[nodiscard]] Quaternion Normalized() const noexcept
		{
			Quaternion result(*this);
			result.vector.Normalize();

			return result;
		}

		[[nodiscard]] bool IsUnity() const noexcept { return vector.IsUnity(); }

		[[nodiscard]] Quaternion Multiply(const Quaternion& rhs) const noexcept
		{
			Quaternion result(nullptr);

			result.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
			result.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
			result.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
			result.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;

			return result;
		}

		[[nodiscard]] TVec3 Multiply(const TVec3& rhs) const noexcept
		{
			Quaternion qv(nullptr);

			qv.x = w * rhs.x + x + y * rhs.z - z * rhs.y;
			qv.y = w * rhs.y - x * rhs.z + y + z * rhs.x;
			qv.z = w * rhs.z + x * rhs.y - y * rhs.x + z;
			qv.w = w - x * rhs.x - y * rhs.y - z * rhs.z;

			TVec3 result(nullptr);
			result.x = -qv.w * x + qv.x * w - qv.y * z + qv.z * y;
			result.y = -qv.w * y + qv.x * z + qv.y * w - qv.z * x;
			result.z = -qv.w * z - qv.x * y + qv.y * x + qv.z * w;

			return result;
		}

		void SetEulerAngles(float x, float y, float z) noexcept
		{
			float sx = RotationSin(DegreeToRadian(x) * 0.5f);
			float sy = RotationSin(DegreeToRadian(y) * 0.5f);
			float sz = RotationSin(DegreeToRadian(z) * 0.5f);

			float cx = RotationCos(DegreeToRadian(x) * 0.5f);
			float cy = RotationCos(DegreeToRadian(y) * 0.5f);
			float cz = RotationCos(DegreeToRadian(z) * 0.5f);

			This::w = cx * cy * cz + sx * sy * sz;
			This::x = sx * cy * cz - cx * sy * sz;
			This::y = cx * sy * cz + sx * cy * sz;
			This::z = cx * cy * sz - sx * sy * cz;
		}

		void SetEulerX(float x) noexcept
		{
			This::w = RotationCos(DegreeToRadian(x) * 0.5f);
			This::x = RotationSin(DegreeToRadian(x) * 0.5f);
			This::y = 0.0f;
			This::z = 0.0f;
		}

		void SetEulerY(float y) noexcept
		{
			This::w = RotationCos(DegreeToRadian(y) * 0.5f);
			This::x = 0.0f;
			This::y = RotationSin(DegreeToRadian(y) * 0.5f);
			This::z = 0.0f;
		}

		void SetEulerZ(float z) noexcept
		{
			This::w = RotationCos(DegreeToRadian(z) * 0.5f);
			This::x = 0.0f;
			This::y = 0.0f;
			This::z = RotationSin(DegreeToRadian(z) * 0.5f);
		}

		void SetEulerXY(float x, float y) noexcept
		{
			float sx = RotationSin(DegreeToRadian(x) * 0.5f);
			float sy = RotationSin(DegreeToRadian(y) * 0.5f);

			float cx = RotationCos(DegreeToRadian(x) * 0.5f);
			float cy = RotationCos(DegreeToRadian(y) * 0.5f);

			This::w = cx * cy;
			This::x = sx * cy;
			This::y = cx * sy;
			This::z = -sx * sy;
		}

		void SetEulerXZ(float x, float z) noexcept
		{
			float sx = RotationSin(DegreeToRadian(x) * 0.5f);
			float sz = RotationSin(DegreeToRadian(z) * 0.5f);

			float cx = RotationCos(DegreeToRadian(x) * 0.5f);
			float cz = RotationCos(DegreeToRadian(z) * 0.5f);

			This::w = cx * cz;
			This::x = sx * cz;
			This::y = sx * sz;
			This::z = cx * sz;
		}

		void SetEulerYZ(float y, float z) noexcept
		{
			float sy = RotationSin(DegreeToRadian(y) * 0.5f);
			float sz = RotationSin(DegreeToRadian(z) * 0.5f);

			float cy = RotationCos(DegreeToRadian(y) * 0.5f);
			float cz = RotationCos(DegreeToRadian(z) * 0.5f);

			This::w = cy * cz;
			This::x = -sy * sz;
			This::y = sy * cz;
			This::z = cy * sz;
		}

		[[nodiscard]] TVec3 EulerAngles() const noexcept
		{
			TVec3 angle(nullptr);

			const float yy = y * y;
			angle.x = RadianToDegree(RotationAtan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + yy)));

			const auto sinp = 2.0f * (w * y - x * z);
			angle.y = std::abs(sinp) >= 1 ? std::copysign(HalfPi, sinp) : RadianToDegree(RotationAsin(sinp));
			angle.z = RadianToDegree(RotationAtan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (yy + z * z)));

			return angle;
		}

		[[nodiscard]] Quaternion LerpTo(Quaternion to, float t) noexcept { return Lerp(*this, to, t); }

		[[nodiscard]] static This Lerp(const This& from, const This& to, float t) noexcept
		{
			This result = from.vector * (1.0f - t) + to.vector * t;
			result.Normalize();

			return result;
		}

		[[nodiscard]] Quaternion SlerpTo(Quaternion to, float t) noexcept { return Slerp(*this, to, t); }

		[[nodiscard]] static This Slerp(const This& from, const This& to, float t) noexcept
		{
			AssertMessage(from.IsUnity(), "Quaternion slerp should have unit length", ", but ", from.vector.Length());
			AssertMessage(to.IsUnity(), "Quaternion slerp should have unit length", ", but ", to.vector.Length());

			auto angle = std::acos(from.vector.Dot(to.vector));

			if (angle < Epsilon)
			{
				return Lerp(from, to, t);
			}

			const auto nt = 1.0f - t;
			const auto sinA = static_cast<TNumber>(std::sin(nt * angle));
			const auto sinB = static_cast<TNumber>(std::sin(t * angle));

			return (from.vector * sinA + to.vector * sinB) / std::sin(angle);
		}

		void LookAt(const TVec3& forward, const TVec3& up) noexcept
		{
			TMat3x3 lookMat(nullptr);
			lookMat.LookAt(forward, up);
			*this = Quaternion(lookMat);
		}

		void SetRotationAround(const TVec3& unitAxis, float radian) noexcept
		{
			Assert(unitAxis.IsUnity(), "Quaternion Length = ", unitAxis.Length());
			w = RotationCos(radian * 0.5f);
			v = unitAxis * RotationSin(radian * 0.5f);
		}

		void SetRotationFromTo(const TVec3& from, const TVec3& to) noexcept
		{
			if (from == to)
			{
				vector = Quaternion::Identity.vector;

				return;
			}

			auto axis = from.Cross(to);
			axis.Normalize();

			SetRotationAround(axis, from.AngleTo(to));
		}
	};

	template<typename T>
	const Quaternion<T> Quaternion<T>::Zero(0, 0, 0, 0);
	template<typename T>
	const Quaternion<T> Quaternion<T>::Identity(0, 0, 0, 1);

	using TQuat = Quaternion<float>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const Quaternion<T>& q) noexcept
	{
		Assert(q.IsUnity(), "Quaternion is not a unit. length = ", q.vector.Length());

		Vector3<T> e = q.EulerAngles();
		os << "Quat (" << e.x << ", " << e.y << ", " << e.z << ")";

		return os;
	}
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class QuaternionTest final : public TestCollection
	{
	public:
		QuaternionTest() : TestCollection("Quaternion Test") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
