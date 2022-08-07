// Created by mooming.go@gmail.com, 2017

#pragma once

#include "MathUtil.h"
#include <ostream>


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

    inline Vector2() : This(0, 0)
    {
    }

    inline Vector2(Number x, Number y) : x(x), y(y)
    {
    }

    inline Vector2(std::nullptr_t)
    {
    }

#include "VectorCommonImpl.inl"
  };

  template <typename T> const Vector2<T> Vector2<T>::Zero(0, 0);
  template <typename T> const Vector2<T> Vector2<T>::Unity(1, 1);
  template <typename T> const Vector2<T> Vector2<T>::X(1, 0);
  template <typename T> const Vector2<T> Vector2<T>::Y(0, 1);
  template <typename T> const Vector2<T> Vector2<T>::Forward(1, 0);
  template <typename T> const Vector2<T> Vector2<T>::Up(0, 1);

  using Int2 = Vector2<int>;
  using Float2 = Vector2<float>;

  template <typename T>
  inline Vector2<T> operator*(T value, Vector2<T> vector)
  {
    return vector * value;
  }

  inline std::ostream& operator<<(std::ostream& os, const Float2& vec)
  {
    os << "(" << vec.x << ", " << vec.y << "), norm = " << vec.Length();
    return os;
  }
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{

  class Vector2Test : public TestCase
  {
  public:

    Vector2Test() : TestCase("Vector2Test")
    {
    }

  protected:
    virtual bool DoTest() override;
  };
} // HE
#endif //__UNIT_TEST__
