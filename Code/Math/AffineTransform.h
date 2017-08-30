// Copyright, All rights reserved by mooming.go@gmail.com

#ifndef AffineTransform_h
#define AffineTransform_h

#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"

namespace HE
{

  template <typename Number>
  class AffineTransform
  {

    using This = AffineTransform;
    using Vec3 = Vector3<Number>;
    using Quat = Quaternion<Number>;
    using Mat3x3 = Matrix3x3<Number>;
    using Mat4x4 = Matrix4x4<Number>;

  public:
    Mat3x3 linearTr;
    Vec3 translation;

  public:

    AffineTransform() : linearTr(), translation()
    {
    }

    AffineTransform(std::nullptr_t) : linearTr(nullptr), translation(nullptr)
    {
    }

    AffineTransform(const Vec3& translation, const Quat& rotation, const Vec3& scale)
      : linearTr(nullptr), translation(translation)
    {
      SetRotationScale(rotation, scale);
    }

    Vec3 operator*(const Vec3& rhs) const
    {
      return Transform(rhs);
    }

    This operator*(const This& rhs) const
    {
      return Transform(rhs);
    }

    Vec3 Transform(const Vec3& rhs) const
    {
      return (linearTr * rhs) + translation;
    }

    This Transform(const This& rhs) const
    {
      This result = nullptr;

      result.linearTr = linearTr * rhs.linearTr;
      result.translation = Transform(rhs.translation);

      return result;
    }

    This Inverse() const
    {
      This result = nullptr;

      auto invMat = linearTr.Inverse();
      result.linearTr = invMat;
      result.translation = invMat * (- translation);

      return result;
    }

    Vec3 InverseTransform(const Vec3& rhs) const
    {
      auto invMat = linearTr.Inverse();
      return invMat * (rhs - translation);
    }

    This InverseTransform(const This& rhs) const
    {
      This result = nullptr;

      auto invMat = linearTr.Inverse();
      result.linearTr = invMat * rhs.linearTr;
      result.translation = invMat * (rhs.translation - translation);

      return result;
    }

    Quat Rotation() const
    {
      auto f = linearTr * Vec3::Forward;
      f.Normalize();

      auto u = linearTr * Vec3::Up;
      u.Normalize();

      return Quat::LookRotation(f, u);
    }

    Vec3 Scale() const
    {
      auto cx = linearTr.Column(0).Length();
      auto cy = linearTr.Column(1).Length();
      auto cz = linearTr.Column(2).Length();

      return Vec3(cx, cy, cz);
    }

    Mat4x4 ToMatrix() const
    {
      Mat4x4 matrix(linearTr);
      matrix.SetTranslation(translation);

      return matrix;
    }

    inline operator Mat4x4() const
    {
      return ToMatrix();
    }

    void SetRotationScale(const Quat& rotation, const Vec3& scale)
    {
      AssertMessage(!IsZero(scale.x) && !IsZero(scale.y) && !IsZero(scale.z)
        , "Every element of scale should not be zero. ", scale);
      linearTr = rotation.ToMat3x3() * Mat3x3::CreateDiagonal(scale);
    }

    void SetTRS(const Vec3& t, const Quat& r, const Vec3& s)
    {
      SetRotationScale(r, s);
      SetTranslation(t);
    }

    void SetRotation(const Quat& rotation)
    {
      SetRotationScale(rotation, Scale());
    }

    void SetScale(const Vec3& scale)
    {
      SetRotationScale(Rotation(), scale);
    }

    void SetTranslation(const Vec3& translation)
    {
      This::translation = translation;
    }
  };

  using AffineTRS = AffineTransform<float>;

  template <typename T>
  inline std::ostream& operator<<(std::ostream& os, const AffineTransform<T>& transform)
  {
    using std::endl;

    os << "Affine Transform" << endl;

    auto t = transform.translation;
    auto r = transform.Rotation().EulerAngles();
    auto s = transform.Scale();

    os << "Position " << t << endl;
    os << "Rotation " << r << endl;
    os << "Scale " << s << endl;

    return os;
  }
}

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{

  class AffineTransformTest : public TestCase
  {

  public:

    AffineTransformTest() : TestCase("AffineTransformTest")
    {
    }

  protected:
    virtual bool DoTest() override;
  };
}
#endif //__UNIT_TEST__

#endif //AffineTransform_h
