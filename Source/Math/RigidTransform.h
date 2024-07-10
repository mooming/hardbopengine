// Created by mooming.go@gmail.com

#pragma once

#include "Matrix3x3.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Vector3.h"

namespace HE
{
template <typename Number = float, int PaddingSize = 8>
class RigidTransform
{
    using This = RigidTransform;
    using Vec3 = Vector3<Number>;
    using Quat = Quaternion<Number>;
    using Mat3x3 = Matrix3x3<Number>;
    using Mat4x4 = Matrix4x4<Number>;

  public:
    Quat rotation;
    Vec3 translation;
    uint8_t padding[PaddingSize];

  public:
    RigidTransform();
    RigidTransform(std::nullptr_t);
    RigidTransform(const Vec3& translation, const Quat& rotation);
    RigidTransform(const Mat4x4& mat);

    inline bool operator==(const This& rhs) const
    {
        return rotation == rhs.rotation && translation == rhs.translation;
    }

    template <typename T>
    inline T operator*(const T& rhs) const
    {
        return Transform(rhs);
    }

    Vec3 Transform(const Vec3& x) const { return rotation * x + translation; }

    Quat Transform(const Quat& r) const { return rotation * r; }

    This Transform(const This& rhs) const
    {
        This result(nullptr);

        result.rotation = rotation * rhs.rotation;
        result.translation = rotation * rhs.translation + translation;

        return result;
    }

    Vec3 InverseTransform(const Vec3& x) const { return (rotation.Inverse() * (x - translation)); }

    Quat InverseTransform(const Quat& r) const { return rotation.Inverse() * r; }

    This InverseTransform(const This& rhs) const
    {
        This result(nullptr);

        result.rotation = InverseTransform(rhs.rotation);
        result.translation = InverseTransform(rhs.translation);

        return result;
    }

    This Inverse() const
    {
        This inverse = nullptr;

        inverse.rotation = rotation.Inverse();
        inverse.translation = inverse.rotation * -translation;

        return inverse;
    }

    Mat4x4 ToMatrix() const
    {
        Mat4x4 mat = rotation.ToMat4x4();
        mat.SetTranslation(translation);

        return mat;
    }
};

using RigidTR = RigidTransform<float>;

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const RigidTransform<T>& local)
{
    using namespace std;

    os << "Rigid Transform" << endl;
    os << "Position: (" << local.translation.x << ", " << local.translation.y << ", "
       << local.translation.z << ")" << endl;
    auto r = local.rotation.EulerAngles();
    os << "Rotation: (" << r.x << ", " << r.y << ", " << r.z << ")" << endl;

    return os;
}

template <typename T, int N>
inline RigidTransform<T, N>::RigidTransform() : rotation(), translation()
{
}

template <typename T, int N>
inline RigidTransform<T, N>::RigidTransform(std::nullptr_t)
    : rotation(nullptr), translation(nullptr)
{
}

template <typename T, int N>
inline RigidTransform<T, N>::RigidTransform(const Vec3& t, const Quat& r)
    : rotation(r), translation(t)
{
}

template <typename T, int N>
inline RigidTransform<T, N>::RigidTransform(const Mat4x4& mat)
    : rotation(mat), translation(mat.m14, mat.m24, mat.m34)
{
    Assert(mat.IsOrthogonal());
}

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{
class RigidTransformTest : public TestCollection
{
  public:
    RigidTransformTest() : TestCollection("RigidTransformTest") {}

  protected:
    virtual void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
