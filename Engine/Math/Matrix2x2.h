// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include "Core/Debug.h"
#include "Vector2.h"

namespace hbe
{
	/// @brief A 2x2 matrix template class for 2D transformations.
	template<typename TNumber>
	class Matrix2x2 final
	{
		using This = Matrix2x2;
		using TVec = Vector2<TNumber>;

	public:
		constexpr static int row = 2;
		constexpr static int column = 2;
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
				float m11, m12;
				float m21, m22;
			};
		};

	public:
		Matrix2x2() noexcept : element{1, 0, 0, 1} {}

		explicit Matrix2x2(std::nullptr_t) noexcept {}

		explicit Matrix2x2(std::array<TNumber, numberOfElements>&& values) noexcept : element(std::move(values)) {}

		[[nodiscard]] This Inverse() const noexcept
		{
			This result;

			const TNumber det = Determinant();
			FatalAssert(row == column && det != 0, "The matrix is not invertible.");

			const TNumber invDet = static_cast<TNumber>(1) / det;
			result.m11 = invDet * m22;
			result.m22 = invDet * m11;
			result.m12 = -invDet * m12;
			result.m21 = -invDet * m21;

			return result;
		}

		void Transpse() noexcept { std::swap(m12, m21); }

		[[nodiscard]] TNumber Determinant() const noexcept { return (m11 * m22) - (m12 * m21); }

		[[nodiscard]] bool IsOrthogonal() const noexcept
		{
			return IsZero(rows[0].Dot(rows[1])) && rows[0].IsUnity() && rows[1].IsUnity();
		}

#include "MatrixCommonImpl.inl"
	};

	template<typename T>
	const Matrix2x2<T> Matrix2x2<T>::Zero({0, 0, 0, 0});
	template<typename T>
	const Matrix2x2<T> Matrix2x2<T>::Identity;

	using TFloat2x2 = Matrix2x2<float>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const Matrix2x2<T>& mat) noexcept
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
