// Created by mooming.go@gmail.com

#pragma once

#include "Math/MathUtil.h"
#include "Vector2.h"
#include "Vector3.h"

namespace HE
{

template <typename Number>
class Vector4
{
    using This = Vector4;
    using Vec2 = Vector2<Number>;
    using Vec3 = Vector3<Number>;

public:
    static constexpr int order = 4;

    static const This Zero;
    static const This Unity;
    static const This X;
    static const This Y;
    static const This Z;
    static const This W;

    static const This Right;
    static const This Up;
    static const This Forward;

    union
    {
        struct
        {
            Number x;
            Number y;
            Number z;
            Number w;
        };

        Number a[order];
    };

public:
    inline Vector4()
        : This(0, 0, 0, 0)
    {
    }

    inline Vector4(std::nullptr_t) {}

    inline Vector4(Number x, Number y, Number z = 0, Number w = 1)
        : x(x),
          y(y),
          z(z),
          w(w)
    {
    }

    inline Vector4(const Vec2& v, Number z = 0, Number w = 1)
        : x(v.x),
          y(v.y),
          z(z),
          w(w)
    {
    }

    inline Vector4(const Vec3& v, Number w = 1)
        : x(v.x),
          y(v.y),
          z(v.z),
          w(w)
    {
    }

    inline This& operator=(const Vec2& v)
    {
        x = v.x;
        y = v.y;

        return *this;
    }

    inline This& operator=(const Vec3& v)
    {
        x = v.x;
        y = v.y;
        z = v.z;

        return *this;
    }

    inline operator Vec2&() { return reinterpret_cast<Vec2&>(*this); }

    inline operator const Vec2&() const
    {
        return reinterpret_cast<const Vec2&>(*this);
    }

    inline operator Vec3&() { return reinterpret_cast<Vec3&>(*this); }

    inline operator const Vec3&() const
    {
        return reinterpret_cast<const Vec3&>(*this);
    }

#include "VectorCommonImpl.inl"
};

template <typename T>
const Vector4<T> Vector4<T>::Zero(0, 0, 0, 0);
template <typename T>
const Vector4<T> Vector4<T>::Unity(1, 1, 1, 1);
template <typename T>
const Vector4<T> Vector4<T>::X(1, 0, 0, 0);
template <typename T>
const Vector4<T> Vector4<T>::Y(0, 1, 0, 0);
template <typename T>
const Vector4<T> Vector4<T>::Z(0, 0, 1, 0);
template <typename T>
const Vector4<T> Vector4<T>::W(0, 0, 0, 1);

#ifdef __RIGHT_HANDED__
template <typename T>
const Vector4<T> Vector4<T>::Right(1.0f, 0.0f, 0.0f);
template <typename T>
const Vector4<T> Vector4<T>::Forward(0.0f, 1.0f, 0.0f);
template <typename T>
const Vector4<T> Vector4<T>::Up(0.0f, 0.0f, 1.0f);
#endif //__RIGHT_HANDED__

#ifdef __LEFT_HANDED__
template <typename T>
const Vector4<T> Vector4<T>::Right(1.0f, 0.0f, 0.0f);
template <typename T>
const Vector4<T> Vector4<T>::Up(0.0f, 1.0f, 0.0f);
template <typename T>
const Vector4<T> Vector4<T>::Forward(0.0f, 0.0f, 1.0f);
#endif //__LEFT_HANDED__

using Float4 = Vector4<float>;

template <typename Number>
inline Vector4<Number> operator*(Number value, Vector4<Number> vector)
{
    return vector * value;
}

template <class TOutStream>
inline TOutStream& operator<<(TOutStream& os, const Float4& v)
{
    os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w
       << "), norm = " << v.Length();
    return os;
}
} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class Vector4Test : public TestCollection
{
public:
    Vector4Test()
        : TestCollection("Vector4Test")
    {
    }

protected:
    virtual void Prepare() override;
};
} // namespace HE
#endif //__UNIT_TEST__
