// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef Vector3_h
#define Vector3_h

#include "Vector2.h"
#include "CoordinateOrientation.h"
#include "System/CommonUtil.h"

#include <cmath>
#include <ostream>

namespace HE
{
    template <typename Number>
    class Vector3
    {
        using This = Vector3;
        using Vec2 = Vector2<Number>;

    public:
        static constexpr int order = 3;
        static const This Zero;
        static const This Unity;
        static const This X;
        static const This Y;
        static const This Z;
        static const This Right;
        static const This Up;
        static const This Forward;

    public:
        union
        {
            struct
            {
                Number x;
                Number y;
                Number z;
            };

            Number a[order];
        };

    public:
        inline Vector3() : This(0, 0, 0)
        {
        }

        inline Vector3(std::nullptr_t)
        {
        }

        inline Vector3(Number x, Number y) : This(x, y, 0)
        {
        }

        inline Vector3(Number x, Number y, Number z) : x(x), y(y), z(z)
        {
        }

        inline Vector3(const Vec2& v, Number z = 0) : x(v.x), y(v.y), z(z)
        {
        }

        inline This& operator= (const Vec2& v)
        {
            x = v.x;
            y = v.y;

            return *this;
        }

        inline operator Vec2& ()
        {
            return reinterpret_cast<Vec2&>(*this);
        }

        inline operator const Vec2& () const
        {
            return reinterpret_cast<const Vec2&>(*this);
        }

#include "VectorCommonImpl.inl"

    public:
        inline Vector3 Cross(const Vector3& rhs) const
        {
            Vector3 result(nullptr);

            result.x = y * rhs.z - z * rhs.y;
            result.y = z * rhs.x - x * rhs.z;
            result.z = x * rhs.y - y * rhs.x;

            return result;
        }

        inline float AngleTo(const Vector3& to) const
        {
            Assert(!IsZero());
            Assert(!to.IsZero());

            return std::acos(Dot(to) / sqrtf(SqrLength() * to.SqrLength()));
        }
    };

    template <typename T> const Vector3<T> Vector3<T>::Zero(0, 0, 0);
    template <typename T> const Vector3<T> Vector3<T>::Unity(1, 1, 1);
    template <typename T> const Vector3<T> Vector3<T>::X(1, 0, 0);
    template <typename T> const Vector3<T> Vector3<T>::Y(0, 1, 0);
    template <typename T> const Vector3<T> Vector3<T>::Z(0, 0, 1);

#ifdef __RIGHT_HANDED__
    template <typename T> const Vector3<T> Vector3<T>::Right(1.0f, 0.0f, 0.0f);
    template <typename T> const Vector3<T> Vector3<T>::Forward(0.0f, 1.0f, 0.0f);
    template <typename T> const Vector3<T> Vector3<T>::Up(0.0f, 0.0f, 1.0f);
#endif //__RIGHT_HANDED__

#ifdef __LEFT_HANDED__
    template <typename T> const Vector3<T> Vector3<T>::Right(1.0f, 0.0f, 0.0f);
    template <typename T> const Vector3<T> Vector3<T>::Up(0.0f, 1.0f, 0.0f);
    template <typename T> const Vector3<T> Vector3<T>::Forward(0.0f, 0.0f, 1.0f);
#endif //__LEFT_HANDED__


    using Int3 = Vector3<int>;
    using Float3 = Vector3<float>;

    template <typename Number>
    inline Vector3<Number> operator* (Number value, Vector3<Number> vector)
    {
        return vector * value;
    }

    template <typename Number>
    inline std::ostream& operator<< (std::ostream& os, const Vector3<Number>& v)
    {
        os << "(" << v.x << ", " << v.y << ", " << v.z << "), norm = " << v.Length();
        return os;
    }
}

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{
    class Vector3Test : public TestCase
    {
    public:
        Vector3Test() : TestCase("Vector3Test") {}
        
    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__

#endif /* Vector3_h */
