// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef UniformTransform_h
#define UniformTransform_h

#include "Vector3.h"
#include "Quaternion.h"
#include "Matrix3x3.h"
#include "Matrix4x4.h"

namespace HE
{
    template <typename Number>
    class UniformTransform
    {
        using This = UniformTransform;
        using Vec3 = Vector3<Number>;
        using Quat = Quaternion<Number>;
        using Mat3x3 = Matrix3x3<Number>;
        using Mat4x4 = Matrix4x4<Number>;

    public:
        Quat rotation;
        Vec3 translation;
        Number scale;

    public:
        UniformTransform();
        UniformTransform(std::nullptr_t);
        UniformTransform(const Vec3& translation, const Quat& rotation, Number scale);
        UniformTransform(const Mat4x4& mat);

        inline bool operator== (const This& rhs)
        {
            return rotation == rhs.rotation && scale == rhs.scale && translation == rhs.translation;
        }

        inline bool operator!= (const This& rhs)
        {
            return !(*this == rhs);
        }

        template <typename T>
        inline T operator* (const T& rhs) const
        {
            return Transform(rhs);
        }

        inline operator Mat3x3 () const;
        inline operator Mat4x4 () const;

        Vec3 Transform(const Vec3& x) const
        {
            return rotation * (scale * x) + translation;
        }

        Quat Transform(const Quat& r) const
        {
            return rotation * r;
        }

        Number Transform(Number s) const
        {
            return scale * s;
        }

        This Transform(const This& rhs) const
        {
            This result(nullptr);

            result.scale = scale * rhs.scale;
            result.rotation = rotation * rhs.rotation;
            result.translation = rotation * (scale * rhs.translation) + translation;

            return result;
        }

        Vec3 InverseTransform(const Vec3& x) const
        {
            return (rotation.Inverse() * (x - translation) / scale) ;
        }

        Quat InverseTransform(const Quat& r) const
        {
            return rotation.Inverse() * r;
        }

        Number InverseTransform(Number s) const
        {
            return s / scale;
        }

        This Inverse() const
        {
            This inverse(nullptr);

            scale = 1.0f / scale;
            rotation.Invert();
            translation = rotation * translation * (-scale);

            return inverse;
        }

        Mat4x4 GetTRS() const
        {
            Mat4x4 mat = rotation.ToMat4x4() * Mat4x4::CreateDiagonal(Float4(scale, scale, scale, 1.0f));
            mat.SetTranslation(translation);

            return mat;
        }

        Mat3x3 GetRS() const
        {
            return rotation.ToMat3x3() * Mat3x3::CreateDiagonal(Vec3(scale, scale, scale));
        }

        Mat3x3 GetR() const
        {
            return rotation.ToMat3x3();
        }
    };

    using UniformTRS = UniformTransform<float>;

    template <typename T>
    inline std::ostream& operator<< (std::ostream& os, const UniformTransform<T>& local)
    {
        using namespace std;

        os << "Uniform Transform" << endl;
        os << "Position: (" << local.translation.x << ", "
            << local.translation.y << ", "
            << local.translation.z << ")" << endl;
        auto r = local.rotation.EulerAngles();
        os << "Rotation: (" << r.x << ", " << r.y << ", " << r.z << ")" << endl;
        os << "Scale: (" << local.scale << ")" << endl;

        return os;
    }

    template<typename Number>
    inline UniformTransform<Number>::UniformTransform()
        : rotation(), translation(), scale(1)
    {
    }

    template<typename Number>
    inline UniformTransform<Number>::UniformTransform(std::nullptr_t)
        : rotation(nullptr), translation(nullptr)
    {
    }

    template<typename Number>
    inline UniformTransform<Number>::UniformTransform(const Vec3& t, const Quat& r, Number s)
        : rotation(r), translation(t), scale(s)
    {
    }

    template<typename Number>
    inline UniformTransform<Number>::UniformTransform(const Mat4x4& mat)
    {
        Assert(mat.IsOrthogonal());

        Vec3 c1 = Vec3(mat.m11, mat.m21, mat.m31);
        Vec3 c2 = Vec3(mat.m12, mat.m22, mat.m32);
        Vec3 c3 = Vec3(mat.m13, mat.m23, mat.m33);

        constexpr float oneThird = 1.0f / 3.0f;
        scale = (c1.Normalize() + c2.Normalize() + c3.Normalize()) * oneThird;

        Assert(IsEqual(scale, c2.Normalize()));
        Assert(IsEqual(scale, c3.Normalize()));

        Mat3x3 rotMat = nullptr;

        rotMat.m11 = c1.x;
        rotMat.m21 = c1.y;
        rotMat.m31 = c1.z;

        rotMat.m12 = c2.x;
        rotMat.m22 = c2.y;
        rotMat.m32 = c2.z;

        rotMat.m13 = c3.x;
        rotMat.m23 = c3.y;
        rotMat.m33 = c3.z;

        rotation = static_cast<Quat>(rotMat);
    }

    template<typename Number>
    inline UniformTransform<Number>::operator Mat3x3() const
    {
        return GetRS();
    }

    template<typename Number>
    inline UniformTransform<Number>::operator Mat4x4() const
    {
        return GetTRS();
    }
}

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{
    class UniformTransformTest : public TestCase
    {
    public:
        UniformTransformTest() : TestCase("UniformTransformTest") {}
        
    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__

#endif //UniformTransform_h
