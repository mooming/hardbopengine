// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <iostream>
#include "CoordinateOrientation.h"
#include "Matrix2x2.h"
#include "Vector3.h"

namespace hbe
{
	/// @brief A 3x3 matrix template class for 3D transformations.
	template<typename TNumber>
	class Matrix3x3 final
	{
		using This = Matrix3x3;
		using TVec = Vector3<TNumber>;
		using TMat2x2 = Matrix2x2<TNumber>;

	public:
		constexpr static int row = 3;
		constexpr static int column = 3;
		constexpr static int numberOfElements = row * column;

		const static This Zero;
		const static This Identity;

		union
		{
			struct
			{
				TVec rows[row];
			};

			TNumber m[row][column];
			std::array<TNumber, numberOfElements> element;

			struct
			{
				float m11, m12, m13;
				float m21, m22, m23;
				float m31, m32, m33;
			};
		};

	public:
		[[nodiscard]] static This createRotation(float x, float y, float z) noexcept
		{
			Matrix3x3 mat(nullptr);
			mat.setEulerAngles(x, y, z);

			return mat;
		}

		[[nodiscard]] static This createRotation(const TVec euler) noexcept
		{
			Matrix3x3 mat(nullptr);
			mat.setEulerAngles(euler);

			return mat;
		}

		Matrix3x3() noexcept : element{1, 0, 0, 0, 1, 0, 0, 0, 1} {}

		explicit Matrix3x3(std::nullptr_t) noexcept {}

		explicit Matrix3x3(std::array<TNumber, numberOfElements>&& values) noexcept : element(std::move(values)) {}

		explicit Matrix3x3(const TMat2x2& rhs) noexcept
		{
			m11 = rhs.m11;
			m12 = rhs.m12;
			m13 = 0.0f;

			m21 = rhs.m21;
			m22 = rhs.m22;
			m23 = 0.0f;

			m31 = 0.0f;
			m32 = 0.0f;
			m33 = 1.0f;
		}

		This& operator=(const TMat2x2& rhs) noexcept
		{
			m11 = rhs.m11;
			m12 = rhs.m12;

			m21 = rhs.m21;
			m22 = rhs.m22;

			return *this;
		}

		explicit operator TMat2x2() const noexcept
		{
			TMat2x2 mat(nullptr);
			mat.m11 = m11;
			mat.m12 = m12;
			mat.m21 = m21;
			mat.m22 = m22;

			return mat;
		}

		[[nodiscard]] This inverse() const noexcept
		{
			This temp;

			const TNumber det = determinant();
			Assert(row == column && det != 0, "The matrix is not invertible.");

			const TNumber invDet = static_cast<TNumber>(1) / det;

			temp.m11 = invDet * (m22 * m33 - m23 * m32);
			temp.m12 = invDet * (m13 * m32 - m12 * m33);
			temp.m13 = invDet * (m12 * m23 - m13 * m22);

			temp.m21 = invDet * (m23 * m31 - m21 * m33);
			temp.m22 = invDet * (m11 * m33 - m13 * m31);
			temp.m23 = invDet * (m13 * m21 - m11 * m23);

			temp.m31 = invDet * (m21 * m32 - m22 * m31);
			temp.m32 = invDet * (m12 * m31 - m11 * m32);
			temp.m33 = invDet * (m11 * m22 - m12 * m21);

			return temp.multiply(invDet);
		}

		void transpose() noexcept
		{
			std::swap(m12, m21);
			std::swap(m13, m31);
			std::swap(m23, m32);
		}

		[[nodiscard]] TNumber determinant() const noexcept
		{
			return m11 * m22 * m33 + m12 * m23 * m31 + m13 * m21 * m32 - m11 * m23 * m32 - m12 * m21 * m33 -
				   m13 * m22 * m31;
		}

		[[nodiscard]] bool isOrthogonal() const noexcept
		{
			return isZero(rows[0].dot(rows[1])) && isZero(rows[1].dot(rows[2])) && isZero(rows[2].dot(rows[0])) &&
				   rows[0].isUnity() && rows[1].isUnity() && rows[2].isUnity();
		}

		void lookAt(const TVec& forward, TVec up) noexcept
		{
			Assert(forward.isUnity(), "Matrix3x3::LookAt - forward must be unit");
			Assert(up.isUnity(), "Matrix3x3::LookAt - up must be unit");

			const float cosAngle = forward.dot(up);
			Assert((cosAngle * cosAngle) < 1.0f, "Matrix3x3::LookAt - forward and up are parallel");

#ifndef RIGHT_HANDED_COORDINATE
			TVec right = up.cross(forward);
			if (!isZero(cosAngle))
			{
				up = forward.cross(right);
			}

			raws[0] = right;
			raws[1] = up;
			raws[2] = forward;
#endif

#ifdef RIGHT_HANDED_COORDINATE
			TFloat3 right = forward.cross(up);
			if (!isZero(cosAngle))
			{
				up = forward.cross(right);
			}

			rows[0] = right;
			rows[1] = forward;
			rows[2] = up;
#endif

			transpose();
		}

		void setEulerX(float radian) noexcept
		{
			const float c = rotationCos(radian);
			const float s = rotationSin(radian);

			m11 = 1.0f;
			m12 = 0.0f;
			m13 = 0.0f;
			m21 = 0.0f;
			m22 = c;
			m23 = -s;
			m31 = 0.0f;
			m32 = s;
			m33 = c;
		}

		void setEulerY(float radian) noexcept
		{
			const float c = rotationCos(radian);
			const float s = rotationSin(radian);

			m11 = c;
			m12 = 0.0f;
			m13 = s;
			m21 = 0.0f;
			m22 = 1.0f;
			m23 = 0.0f;
			m31 = -s;
			m32 = 0.0f;
			m33 = c;
		}

		void setEulerZ(float radian) noexcept
		{
			const float c = rotationCos(radian);
			const float s = rotationSin(radian);

			m11 = c;
			m12 = -s;
			m13 = 0.0f;
			m21 = s;
			m22 = c;
			m23 = 0.0f;
			m31 = 0.0f;
			m32 = 0.0f;
			m33 = 0.0f;
		}

		void setEulerAngles(const TVec& euler) noexcept { setEulerAngles(euler.x, euler.y, euler.z); }

		void setEulerAngles(float x, float y, float z) noexcept
		{
			const float cx = rotationCos(degreeToRadian(x));
			const float cy = rotationCos(degreeToRadian(y));
			const float cz = rotationCos(degreeToRadian(z));

			const float sx = rotationSin(degreeToRadian(x));
			const float sy = rotationSin(degreeToRadian(y));
			const float sz = rotationSin(degreeToRadian(z));

			m11 = cy * cz;
			m12 = sx * sy * cz - cx * sz;
			m13 = cx * sy * cz + sx * sz;

			m21 = cy * sz;
			m22 = sx * sy * sz + cx * cz;
			m23 = cx * sy * sz - sx * cz;

			m31 = -sy;
			m32 = sx * cy;
			m33 = cx * cy;
		}

#include "MatrixCommonImpl.inl"
	};

	template<typename T>
	const Matrix3x3<T> Matrix3x3<T>::Zero({0, 0, 0, 0, 0, 0, 0, 0, 0});
	template<typename T>
	const Matrix3x3<T> Matrix3x3<T>::Identity;

	using TFloat3x3 = Matrix3x3<float>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const Matrix3x3<T>& mat) noexcept
	{
		using namespace std;
		os << "Matrix " << mat.row << "x" << mat.column << endl;
		for (int i = 0; i < mat.row; ++i)
		{
			os << mat.rows[i].a[0];

			for (int j = 1; j < mat.column; ++j)
			{
				os << ", " << mat.rows[i].a[j];
			}

			os << endl;
		}

		return os;
	}
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class Matrix3x3Test final : public TestCollection
	{
	public:
		Matrix3x3Test() : TestCollection("Matrix3x3Test") {}

	protected:
		void prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
