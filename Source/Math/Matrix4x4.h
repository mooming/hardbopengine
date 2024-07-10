// Created by mooming.go@gmail.com

#pragma once

#include "CoordinateOrientation.h"
#include "Matrix2x2.h"
#include "Matrix3x3.h"
#include "Vector3.h"
#include "Vector4.h"
#include <array>


namespace HE
{

template <typename Number>
class Matrix4x4
{
    using This = Matrix4x4;
    using Vec = Vector4<Number>;

    using Vec3 = Vector3<Number>;
    using Mat2x2 = Matrix2x2<Number>;
    using Mat3x3 = Matrix3x3<Number>;

  public:
    constexpr static int row = 4;
    constexpr static int column = 4;
    constexpr static int numberOfElements = row * column;

    const static This Zero;
    const static This Identity;

    union
    {
        struct
        {
            Vec rows[row];
        };

        Number m[row][column];
        std::array<Number, numberOfElements> element;

        struct
        {
            float m11, m12, m13, m14;
            float m21, m22, m23, m24;
            float m31, m32, m33, m34;
            float m41, m42, m43, m44;
        };
    };

  public:
    inline static This CreateTranslation(const Vec3& v)
    {
        This mat(nullptr);

        mat.m11 = 1;
        mat.m12 = 0;
        mat.m13 = 0;
        mat.m14 = v.x;

        mat.m21 = 0;
        mat.m22 = 1;
        mat.m23 = 0;
        mat.m24 = v.y;

        mat.m31 = 0;
        mat.m32 = 0;
        mat.m33 = 1;
        mat.m34 = v.z;

        mat.m41 = 0;
        mat.m42 = 0;
        mat.m43 = 0;
        mat.m44 = 1;

        return mat;
    }

    inline Matrix4x4()
    {
        m11 = 1;
        m12 = 0;
        m13 = 0;
        m14 = 0;

        m21 = 0;
        m22 = 1;
        m23 = 0;
        m24 = 0;

        m31 = 0;
        m32 = 0;
        m33 = 1;
        m34 = 0;

        m41 = 0;
        m42 = 0;
        m43 = 0;
        m44 = 1;
    }

    inline Matrix4x4(std::nullptr_t) {}

    inline Matrix4x4(const std::array<Number, numberOfElements>& values) : element(values) {}

    inline Matrix4x4(std::array<Number, numberOfElements>&& values) : element(std::move(values)) {}

    inline Matrix4x4(const Mat2x2& rhs)
    {
        m11 = rhs.m11;
        m12 = rhs.m12;
        m13 = 0.0f;
        m14 = 0.0f;

        m21 = rhs.m21;
        m22 = rhs.m22;
        m23 = 0.0f;
        m24 = 0.0f;

        m31 = 0.0f;
        m32 = 0.0f;
        m33 = 1.0f;
        m34 = 0.0f;

        m41 = 0.0f;
        m42 = 0.0f;
        m43 = 0.0f;
        m44 = 1.0f;
    }

    inline Matrix4x4(const Mat3x3& rhs)
    {
        m11 = rhs.m11;
        m12 = rhs.m12;
        m13 = rhs.m13;
        m14 = 0.0f;

        m21 = rhs.m21;
        m22 = rhs.m22;
        m23 = rhs.m23;
        m24 = 0.0f;

        m31 = rhs.m31;
        m32 = rhs.m32;
        m33 = rhs.m33;
        m34 = 0.0f;

        m41 = 0.0f;
        m42 = 0.0f;
        m43 = 0.0f;
        m44 = 1.0f;
    }

    This& operator=(const Mat2x2& rhs)
    {
        m11 = rhs.m11;
        m12 = rhs.m12;

        m21 = rhs.m21;
        m22 = rhs.m22;

        return *this;
    }

    This& operator=(const Mat3x3& rhs)
    {
        m11 = rhs.m11;
        m12 = rhs.m12;
        m13 = rhs.m13;

        m21 = rhs.m21;
        m22 = rhs.m22;
        m23 = rhs.m23;

        m31 = rhs.m31;
        m32 = rhs.m32;
        m33 = rhs.m33;

        return *this;
    }

    inline operator Mat2x2() const
    {
        Mat2x2 mat = nullptr;

        mat.m11 = m11;
        mat.m12 = m12;

        mat.m21 = m21;
        mat.m22 = m22;

        return mat;
    }

    inline operator Mat3x3() const
    {
        Mat3x3 mat = nullptr;

        mat.m11 = m11;
        mat.m12 = m12;
        mat.m13 = m13;

        mat.m21 = m21;
        mat.m22 = m22;
        mat.m23 = m23;

        mat.m31 = m31;
        mat.m32 = m32;
        mat.m33 = m33;

        return mat;
    }

    inline This Inverse() const
    {
        This result;

        const Number det = Determinant();
        FatalAssert(row == column && det != 0, "The matrix is not invertible.");

        const Number invDet = static_cast<Number>(1) / det;

        result.m11 = invDet * (m22 * m33 * m44 + m23 * m34 * m42 + m24 * m32 * m43 -
                               m22 * m34 * m43 - m23 * m32 * m44 - m24 * m33 * m42);
        result.m12 = invDet * (m12 * m34 * m43 + m13 * m32 * m44 + m14 * m33 * m42 -
                               m12 * m33 * m44 - m13 * m34 * m42 - m14 * m32 * m43);
        result.m13 = invDet * (m12 * m23 * m44 + m13 * m24 * m42 + m14 * m22 * m43 -
                               m12 * m24 * m43 - m13 * m22 * m44 - m14 * m23 * m42);
        result.m14 = invDet * (m12 * m24 * m33 + m13 * m22 * m34 + m14 * m23 * m32 -
                               m12 * m23 * m34 - m13 * m24 * m32 - m14 * m22 * m33);
        result.m21 = invDet * (m21 * m34 * m43 + m23 * m31 * m44 + m24 * m33 * m41 -
                               m21 * m33 * m44 - m23 * m34 * m41 - m24 * m31 * m43);
        result.m22 = invDet * (m11 * m33 * m44 + m13 * m34 * m41 + m14 * m31 * m43 -
                               m11 * m34 * m43 - m13 * m31 * m44 - m14 * m33 * m41);
        result.m23 = invDet * (m11 * m24 * m43 + m13 * m21 * m44 + m14 * m23 * m41 -
                               m11 * m23 * m44 - m13 * m24 * m41 - m14 * m21 * m43);
        result.m24 = invDet * (m11 * m23 * m34 + m13 * m24 * m31 + m14 * m21 * m33 -
                               m11 * m24 * m33 - m13 * m21 * m34 - m14 * m23 * m31);
        result.m31 = invDet * (m21 * m32 * m44 + m22 * m34 * m41 + m24 * m31 * m42 -
                               m21 * m34 * m42 - m22 * m31 * m44 - m24 * m32 * m41);
        result.m32 = invDet * (m11 * m34 * m42 + m12 * m31 * m44 + m14 * m32 * m41 -
                               m11 * m32 * m44 - m12 * m34 * m41 - m14 * m31 * m42);
        result.m33 = invDet * (m11 * m22 * m44 + m12 * m24 * m41 + m14 * m21 * m42 -
                               m11 * m24 * m42 - m12 * m21 * m44 - m14 * m22 * m41);
        result.m34 = invDet * (m11 * m24 * m32 + m12 * m21 * m34 + m14 * m22 * m31 -
                               m11 * m22 * m34 - m12 * m24 * m31 - m14 * m21 * m32);
        result.m41 = invDet * (m21 * m33 * m42 + m22 * m31 * m43 + m23 * m32 * m41 -
                               m21 * m32 * m43 - m22 * m33 * m41 - m23 * m31 * m42);
        result.m42 = invDet * (m11 * m32 * m43 + m12 * m33 * m41 + m13 * m31 * m42 -
                               m11 * m33 * m42 - m12 * m31 * m43 - m13 * m32 * m41);
        result.m43 = invDet * (m11 * m23 * m42 + m12 * m21 * m43 + m13 * m22 * m41 -
                               m11 * m22 * m43 - m12 * m23 * m41 - m13 * m21 * m42);
        result.m44 = invDet * (m11 * m22 * m33 + m12 * m23 * m31 + m13 * m21 * m32 -
                               m11 * m23 * m32 - m12 * m21 * m33 - m13 * m22 * m31);

        result.Multiply(invDet);

        return result;
    }

    inline void Transpose()
    {
        std::swap(m12, m21);
        std::swap(m13, m31);
        std::swap(m14, m41);
        std::swap(m23, m32);
        std::swap(m24, m42);
        std::swap(m34, m43);
    }

    inline Number Determinant() const
    {
        return m11 * m22 * m33 * m44 + m11 * m23 * m34 * m42 + m11 * m24 * m32 * m43 +
               m12 * m21 * m34 * m43 + m12 * m23 * m31 * m44 + m12 * m24 * m33 * m41 +
               m13 * m21 * m32 * m44 + m13 * m22 * m34 * m41 + m13 * m24 * m31 * m42 +
               m14 * m21 * m33 * m42 + m14 * m22 * m31 * m43 + m14 * m23 * m32 * m41

               - m11 * m22 * m34 * m43 - m11 * m23 * m32 * m44 - m11 * m24 * m33 * m42 -
               m12 * m21 * m33 * m44 - m12 * m23 * m34 * m41 - m12 * m24 * m31 * m43 -
               m13 * m21 * m34 * m42 - m13 * m22 * m31 * m44 - m13 * m24 * m32 * m41 -
               m14 * m21 * m32 * m43 - m14 * m22 * m33 * m41 - m14 * m23 * m31 * m42;
    }

    inline bool IsOrthogonal() const
    {
        return IsZero(rows[0].Dot(rows[1])) && IsZero(rows[1].Dot(rows[2])) &&
               IsZero(rows[2].Dot(rows[3])) && IsZero(rows[3].Dot(rows[0])) && rows[0].IsUnity() &&
               rows[1].IsUnity() && rows[2].IsUnity() && rows[3].IsUnity();
    }

    void SetTranslation(const Vec3& translation)
    {
        m14 = translation.x;
        m24 = translation.y;
        m34 = translation.z;
    }

    inline void SetRotationX(float radian)
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

    inline void SetRotationY(float radian)
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

    inline void SetRotationZ(float radian)
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

    inline void EulerAngles(float x, float y, float z)
    {
        const float cx = RotationCos(x);
        const float cy = RotationCos(y);
        const float cz = RotationCos(z);

        const float sx = RotationSin(x);
        const float sy = RotationSin(y);
        const float sz = RotationSin(z);

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

template <typename T>
const Matrix4x4<T> Matrix4x4<T>::Zero({
    0,
});
template <typename T>
const Matrix4x4<T> Matrix4x4<T>::Identity;

using Float4x4 = Matrix4x4<float>;

template <typename T>
std::ostream& operator<<(std::ostream& os, const Matrix4x4<T>& mat)
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
} // namespace HE
