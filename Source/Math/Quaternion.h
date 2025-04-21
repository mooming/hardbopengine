// Created by mooming.go@gmail.com

#pragma once

#include "CoordinateOrientation.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

namespace HE
{
// Assume every quaternion is a unit quaternion.

template <typename Number>
class Quaternion
{
    using This = Quaternion;
    using Vec3 = Vector3<Number>;
    using Vec4 = Vector4<Number>;
    using Mat3x3 = Matrix3x3<Number>;
    using Mat4x4 = Matrix4x4<Number>;

public:
    static const This Zero;
    static const This Identity;

public:
    union
    {
        struct
        {
            Number x;
            Number y;
            Number z;
            Number w;
        };

        struct
        {
            Vec3 v;
            Number s;
        };

        Vec4 vector;
        Number a[4];
    };

public:
    static inline Quaternion CreateRotationX(const float x)
    {
        Quaternion result(nullptr);
        result.SetEulerX(x);
        return result;
    }

    static inline Quaternion CreateRotationY(const float y)
    {
        Quaternion result(nullptr);
        result.SetEulerY(y);
        return result;
    }

    static inline Quaternion CreateRotationZ(const float z)
    {
        Quaternion result(nullptr);
        result.SetEulerZ(z);
        return result;
    }

    static inline Quaternion CreateRotationXY(float x, float y)
    {
        Quaternion result(nullptr);
        result.SetEulerXY(x, y);
        return result;
    }

    static inline Quaternion CreateRotationYZ(float y, float z)
    {
        Quaternion result(nullptr);
        result.SetEulerYZ(y, z);
        return result;
    }

    static inline Quaternion CreateRotationXZ(float x, float z)
    {
        Quaternion result(nullptr);
        result.SetEulerXZ(x, z);
        return result;
    }

    static inline Quaternion LookRotation(const Vec3& forward, const Vec3& up)
    {
        Quaternion result(nullptr);
        result.LookAt(forward, up);
        return result;
    }

    static inline Quaternion RotationAround(const Vec3& unitAxis, float radian)
    {
        Quaternion result(nullptr);
        result.SetRotationAround(unitAxis, radian);

        return result;
    }

    static inline Quaternion RotationFromTo(const Vec3& from, const Vec3& to)
    {
        Quaternion result(nullptr);
        result.SetRotationFromTo(from, to);

        return result;
    }

    inline Quaternion()
        : v(),
          s(1.0f)
    {
    }

    inline Quaternion(const Vec3& eulerAngles)
        : Quaternion(nullptr)
    {
        SetEulerAngles(eulerAngles.x, eulerAngles.y, eulerAngles.z);
    }

    inline Quaternion(float x, float y, float z)
        : Quaternion(nullptr)
    {
        SetEulerAngles(x, y, z);
    }

    inline Quaternion(float x, float y, float z, float w)
        : vector(x, y, z, w)
    {
    }

    inline Quaternion(const Vec4& vector)
        : vector(vector)
    {
    }

    inline Quaternion(const Mat3x3& mat)
    {
        Assert(mat.IsOrthogonal(),
            "[Quaternion] Given matrix is not orthogonal!", mat);
        Assert(IsEqual(mat.Determinant(), 1.0f),
            "[Quaternion] Matrix Determinant should be 1, but ",
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

    inline Quaternion(std::nullptr_t) {}

    inline Mat3x3 ToMat3x3() const
    {
        Mat3x3 mat(nullptr);

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

    inline Mat4x4 ToMat4x4() const
    {
        Mat4x4 mat(nullptr);

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

    inline operator Mat3x3() const { return ToMat3x3(); }

    inline operator Mat4x4() const { return ToMat4x4(); }

    inline Vec3 operator*(const Vec3& rhs) const { return Multiply(rhs); }

    Quaternion operator*(const Quaternion& rhs) const { return Multiply(rhs); }

    Quaternion operator/(const Quaternion& rhs) const
    {
        return Multiply(rhs.Inverse());
    }

    Quaternion& operator*=(const Quaternion& rhs)
    {
        *this = Multiply(rhs);
        return *this;
    }

    Quaternion& operator/=(const Quaternion& rhs)
    {
        *this = Multiply(rhs.Inverse());
        return *this;
    }

    bool operator==(const Quaternion& rhs) const
    {
        return vector == rhs.vector;
    }

    bool operator!=(const Quaternion& rhs) const
    {
        return vector != rhs.vector;
    }

    inline void Conjugate() { v.Negate(); }

    inline Quaternion Conjugated() const
    {
        Quaternion result(*this);
        result.Conjugate();
        return result;
    }

    // Invert itself

    inline void Invert()
    {
        Assert(!vector.IsZero());
        Assert(IsEqual(vector.SqrLength(), 1.0f));

        Conjugate();
    }

    // Return the inverse of it

    inline Quaternion Inverse() const
    {
        Quaternion result(*this);
        result.Invert();
        return result;
    }

    inline void Normalize() { vector.Normalize(); }

    inline Quaternion Normalized() const
    {
        Quaternion result(*this);
        result.vector.Normalize();
        return result;
    }

    inline bool IsUnity() const { return vector.IsUnity(); }

    inline Quaternion Multiply(const Quaternion& rhs) const
    {
        Quaternion result(nullptr);

        result.x = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
        result.y = w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x;
        result.z = w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w;
        result.w = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;

        return result;
    }

    inline Vec3 Multiply(const Vec3& rhs) const
    {
        Quaternion qv(nullptr);

        qv.x = w * rhs.x + x + y * rhs.z - z * rhs.y;
        qv.y = w * rhs.y - x * rhs.z + y + z * rhs.x;
        qv.z = w * rhs.z + x * rhs.y - y * rhs.x + z;
        qv.w = w - x * rhs.x - y * rhs.y - z * rhs.z;

        Vec3 result(nullptr);
        result.x = -qv.w * x + qv.x * w - qv.y * z + qv.z * y;
        result.y = -qv.w * y + qv.x * z + qv.y * w - qv.z * x;
        result.z = -qv.w * z - qv.x * y + qv.y * x + qv.z * w;

        return result;
    }

    inline void SetEulerAngles(float x, float y, float z)
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

    inline void SetEulerX(float x)
    {
        This::w = RotationCos(DegreeToRadian(x) * 0.5f);
        This::x = RotationSin(DegreeToRadian(x) * 0.5f);
        This::y = 0.0f;
        This::z = 0.0f;
    }

    inline void SetEulerY(float y)
    {
        This::w = RotationCos(DegreeToRadian(y) * 0.5f);
        This::x = 0.0f;
        This::y = RotationSin(DegreeToRadian(y) * 0.5f);
        This::z = 0.0f;
    }

    inline void SetEulerZ(float z)
    {
        This::w = RotationCos(DegreeToRadian(z) * 0.5f);
        This::x = 0.0f;
        This::y = 0.0f;
        This::z = RotationSin(DegreeToRadian(z) * 0.5f);
    }

    inline void SetEulerXY(float x, float y)
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

    inline void SetEulerXZ(float x, float z)
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

    inline void SetEulerYZ(float y, float z)
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

    inline Vec3 EulerAngles() const
    {
        Vec3 angle(nullptr);

        const float yy = y * y;
        angle.x = RadianToDegree(
            RotationAtan2(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + yy)));

        const auto sinp = 2.0f * (w * y - x * z);
        angle.y = std::abs(sinp) >= 1 ? std::copysign(HalfPi, sinp)
                                      : RadianToDegree(RotationAsin(sinp));
        angle.z = RadianToDegree(
            RotationAtan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (yy + z * z)));

        return angle;
    }

    inline Quaternion LerpTo(Quaternion to, float t)
    {
        return Lerp(*this, to, t);
    }

    inline static This Lerp(const This& from, const This& to, float t)
    {
        This result = from.vector * (1.0f - t) + to.vector * t;
        result.Normalize();

        return result;
    }

    inline Quaternion SlerpTo(Quaternion to, float t)
    {
        return Slerp(*this, to, t);
    }

    inline static This Slerp(const This& from, const This& to, float t)
    {
        AssertMessage(from.IsUnity(),
            "Quaternion slerp should have unit length", ", but ",
            from.vector.Length());
        AssertMessage(to.IsUnity(), "Quaternion slerp should have unit length",
            ", but ", to.vector.Length());

        auto angle = std::acos(from.vector.Dot(to.vector));

        if (angle < Epsilon)
        {
            return Lerp(from, to, t);
        }

        const auto nt = 1.0f - t;
        const auto sinA = static_cast<Number>(std::sin(nt * angle));
        const auto sinB = static_cast<Number>(std::sin(t * angle));

        return (from.vector * sinA + to.vector * sinB) / std::sin(angle);
    }

    inline void LookAt(const Vec3& forward, const Vec3& up)
    {
        Mat3x3 lookMat(nullptr);
        lookMat.LookAt(forward, up);
        *this = Quaternion(lookMat);
    }

    inline void SetRotationAround(const Vec3& unitAxis, float radian)
    {
        Assert(unitAxis.IsUnity(), "Quaternion Length = ", unitAxis.Length());
        w = RotationCos(radian * 0.5f);
        v = unitAxis * RotationSin(radian * 0.5f);
    }

    inline void SetRotationFromTo(const Vec3& from, const Vec3& to)
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

template <typename T>
const Quaternion<T> Quaternion<T>::Zero(0, 0, 0, 0);
template <typename T>
const Quaternion<T> Quaternion<T>::Identity(0, 0, 0, 1);

using Quat = Quaternion<float>;

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Quaternion<T>& q)
{
    Assert(
        q.IsUnity(), "Quaternion is not a unit. length = ", q.vector.Length());

    Vector3<T> e = q.EulerAngles();
    os << "Quat (" << e.x << ", " << e.y << ", " << e.z << ")";
    return os;
}
} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class QuaternionTest : public TestCollection
{
public:
    QuaternionTest()
        : TestCollection("QuaternionTest")
    {
    }

protected:
    virtual void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
