// Created by mooming.go@gmail.com

#pragma once

#include "MathUtil.h"

namespace HE
{

template <typename Number>
class Vector2
{
    using This = Vector2;

public:
    constexpr static int order = 2;
    static const This Zero;
    static const This Unity;
    static const This X;
    static const This Y;
    static const This Forward;
    static const This Up;

    union
    {
        struct
        {
            Number x;
            Number y;
        };

        Number a[order];
    };

public:
    inline Vector2()
        : This(0, 0)
    {
    }

    inline Vector2(Number x, Number y)
        : x(x),
          y(y)
    {
    }

    inline Vector2(std::nullptr_t) {}

#include "VectorCommonImpl.inl"
};

template <typename T>
const Vector2<T> Vector2<T>::Zero(0, 0);
template <typename T>
const Vector2<T> Vector2<T>::Unity(1, 1);
template <typename T>
const Vector2<T> Vector2<T>::X(1, 0);
template <typename T>
const Vector2<T> Vector2<T>::Y(0, 1);
template <typename T>
const Vector2<T> Vector2<T>::Forward(1, 0);
template <typename T>
const Vector2<T> Vector2<T>::Up(0, 1);

using Int2 = Vector2<int>;
using Float2 = Vector2<float>;

template <typename T>
inline Vector2<T> operator*(T value, Vector2<T> vector)
{
    return vector * value;
}

template <class TOutStream>
inline TOutStream& operator<<(TOutStream& os, const Float2& vec)
{
    os << "(" << vec.x << ", " << vec.y << "), norm = " << vec.Length();
    return os;
}
} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class Vector2Test : public TestCollection
{
public:
    Vector2Test()
        : TestCollection("Vector2Test")
    {
    }

protected:
    virtual void Prepare() override;
};
} // namespace HE

#endif //__UNIT_TEST__
