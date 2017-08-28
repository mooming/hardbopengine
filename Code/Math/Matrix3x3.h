// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Matrix3x3_h
#define Matrix3x3_h

#include "CoordinateOrientation.h"
#include "Vector3.h"
#include "Matrix2x2.h"

#include <utility>
#include <array>

namespace HE
{
    template <typename Number>
    class Matrix3x3
    {
        using This = Matrix3x3;
        using Vec = Vector3<Number>;
        using Mat2x2 = Matrix2x2<Number>;

    public:
        constexpr static int raw = 3;
        constexpr static int column = 3;
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
                float m11, m12, m13;
                float m21, m22, m23;
                float m31, m32, m33;
            };
        };

    public:

        static This CreateRotation(float x, float y, float z)
        {
            Matrix3x3 mat(nullptr);
            mat.SetEulerAngles(x, y, z);

            return mat;
        }

        static This CreateRotation(const Vec euler)
        {
            Matrix3x3 mat(nullptr);
            mat.SetEulerAngles(euler);

            return mat;
        }

        inline Matrix3x3()
        {
            m11 = 1;
            m12 = 0;
            m13 = 0;

            m21 = 0;
            m22 = 1;
            m23 = 0;

            m31 = 0;
            m32 = 0;
            m33 = 1;
        }

        inline Matrix3x3(std::nullptr_t)
        {
        }

        inline Matrix3x3(std::array<Number, numberOfElements>&& values) : element(std::move(values))
        {
        }

        inline Matrix3x3(const Mat2x2& rhs)
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

        This& operator= (const Mat2x2& rhs)
        {
            m11 = rhs.m11;
            m12 = rhs.m12;

            m21 = rhs.m21;
            m22 = rhs.m22;

            return *this;
        }

        inline operator Mat2x2 () const
        {
            Mat2x2 mat = nullptr;

            mat.m11 = m11;
            mat.m12 = m12;

            mat.m21 = m21;
            mat.m22 = m22;

            return mat;
        }

        inline This Inverse() const
        {
            This result;

            const Number det = Determinant();
            AssertMessage(raw == column && det != 0, "The matrix is not invertible.");

            const Number invDet = static_cast<Number>(1) / det;

            result.m11 = invDet * (m22 * m33 - m23 * m32);
            result.m12 = invDet * (m13 * m32 - m12 * m33);
            result.m13 = invDet * (m12 * m23 - m13 * m22);

            result.m21 = invDet * (m23 * m31 - m21 * m33);
            result.m22 = invDet * (m11 * m33 - m13 * m31);
            result.m23 = invDet * (m13 * m21 - m11 * m23);

            result.m31 = invDet * (m21 * m32 - m22 * m31);
            result.m32 = invDet * (m12 * m31 - m11 * m32);
            result.m33 = invDet * (m11 * m22 - m12 * m21);

            result.Multiply(invDet);

            return result;
        }

        inline void Transpose()
        {
            std::swap(m12, m21);
            std::swap(m13, m31);
            std::swap(m23, m32);
        }

        inline Number Determinant() const
        {
            return m11 * m22 * m33 + m12 * m23 * m31 + m13 * m21 * m32
                - m11 * m23 * m32 - m12 * m21 * m33 - m13 * m22 * m31;
        }

        inline bool IsOrthogonal() const
        {
            return IsZero(raws[0].Dot(raws[1]))
                && IsZero(raws[1].Dot(raws[2]))
                && IsZero(raws[2].Dot(raws[0]))
                && raws[0].IsUnity()
                && raws[1].IsUnity()
                && raws[2].IsUnity();
        }

        inline void LookAt(const Vec& forward, Vec up)
        {
            Assert(forward.IsUnity());
            Assert(up.IsUnity());

            const float cosAngle = forward.Dot(up);
            Assert((cosAngle * cosAngle) < 1.0f);

#ifdef __LEFT_HANDED__
            Vec right = up.Cross(forward);
            if (!IsZero(cosAngle))
            {
                up = forward.Cross(right);
            }

            raws[0] = right;
            raws[1] = up;
            raws[2] = forward;
#endif //__LEFT_HANDED__

#ifdef __RIGHT_HANDED__
            Float3 right = forward.Cross(up);
            if (!IsZero(cosAngle))
            {
                up = forward.Cross(right);
            }

            raws[0] = right;
            raws[1] = forward;
            raws[2] = up;
#endif //__RIGHT_HANDED__

            Transpose();
        }

        inline void SetEulerX(float radian)
        {
            const float c = RotationCos(radian);
            const float s = RotationSin(radian);

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

        inline void SetEulerY(float radian)
        {
            const float c = RotationCos(radian);
            const float s = RotationSin(radian);

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

        inline void SetEulerZ(float radian)
        {
            const float c = RotationCos(radian);
            const float s = RotationSin(radian);

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

        inline void SetEulerAngles(const Vec& euler)
        {
            SetEulerAngles(euler.x, euler.y, euler.z);
        }

        inline void SetEulerAngles(float x, float y, float z)
        {
            const float cx = RotationCos(DegreeToRadian(x));
            const float cy = RotationCos(DegreeToRadian(y));
            const float cz = RotationCos(DegreeToRadian(z));

            const float sx = RotationSin(DegreeToRadian(x));
            const float sy = RotationSin(DegreeToRadian(y));
            const float sz = RotationSin(DegreeToRadian(z));

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

    using Float3x3 = Matrix3x3<float>;

    template <typename T>
    std::ostream& operator<< (std::ostream& os, const Matrix3x3<T>& mat)
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


#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{
    class Matrix3x3Test : public TestCase
    {
    public:
        Matrix3x3Test() : TestCase("Matrix3x3Test") {}

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__

#endif //Matrix3x3_h
