// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Matrix2x2_h
#define Matrix2x2_h

#include "Vector2.h"
#include "System/Debug.h"

#include <algorithm>
#include <array>

namespace HE
{
	template <typename Number>
	class Matrix2x2
	{
		using This = Matrix2x2;
		using Vec = Vector2<Number>;

	public:
		constexpr static int raw = 2;
		constexpr static int column = 2;
		constexpr static int numberOfElements = raw * column;

		const static This Zero;
		const static This Identity;

		union
		{
			struct
			{
				Vec raws[raw];
			};

			Number m[raw][column];
			std::array<Number, numberOfElements> element;

			struct
			{
				float m11, m12;
				float m21, m22;
			};
		};

	public:
		inline Matrix2x2()
		{
            m11 = 1;
            m12 = 0;

            m21 = 0;
            m22 = 1;
		}

		inline Matrix2x2(std::nullptr_t)
        {
        }

        inline Matrix2x2(std::array<Number, numberOfElements>&& values) : element(std::move(values))
        {
        }

		inline This Inverse() const
		{
			This result;

			const Number det = Determinant();
			FatalAssertMessage(raw == column && det != 0, "The matrix is not invertible.");

			const Number invDet = static_cast<Number>(1) / det;
			result.m11 = invDet * m22;
			result.m22 = invDet * m11;
			result.m12 = -invDet * m12;
			result.m21 = -invDet * m21;

			return result;
		}

		inline void Transpse()
		{
            std::swap(m12, m21);
		}

		inline Number Determinant() const
		{
			return (m11 * m22) - (m12 * m21);
		}

		inline bool IsOrthogonal() const
		{
			return IsZero(raws[0].Dot(raws[1]))
				&& raws[0].IsUnity()
				&& raws[1].IsUnity();
		}

#include "MatrixCommonImpl.inl"

	};

	using Float2x2 = Matrix2x2<float>;

    template <typename T>
    std::ostream& operator<< (std::ostream& os, const Matrix2x2<T>& mat)
    {
        using namespace std;
        os << "Matrix " << mat.raw << " x" << mat.column << endl;
        for (int i = 0; i < mat.raw; ++i)
        {
            os << mat.raws[i].a[0];

            for (int j = 1; j < mat.column; ++j)
            {
                os << ", " << mat.raws[i].a[j];
            }

            os << endl;
        }

        return os;
    }
}

#endif /* Matrix2x2_h */
