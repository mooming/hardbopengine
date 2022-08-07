// Created by mooming.go@gmail.com, 2017

#pragma once

#include "System/Constants.h"
#include "System/Debug.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>


namespace HE
{
  template <typename T>
  inline T Abs(T value, std::false_type)
  {
    return std::abs(value);
  }

  template <typename T>
  inline T Abs(T value, std::true_type)
  {
    constexpr int shift = sizeof (T) * 8 - 1;
    const uint8_t mask = value >> shift;

    return (value + mask) ^ mask;
  }

  template <typename T>
  inline T Abs(T value)
  {
    return Abs<T>(value, std::is_integral<T>());
  }

  template <typename T>
  inline T Pow(T value, T n, std::true_type)
  {
    FatalAssert(n >= 0);

    T result = 1;
    T order = 1;
    T factor = value;

    while (n > 0)
    {
      while (n >= order)
      {
        n -= order;
        result *= factor;
        factor *= factor;
        order *= 2;
      }

      order = 1;
      factor = value;
    }

    return result;
  }

  template <typename T>
  T Pow(T value, T n, std::false_type);

  template <>
  inline float Pow(float value, float n, std::false_type)
  {
    return std::powf(value, n);
  }

  template <>
  inline double Pow(double value, double n, std::false_type)
  {
    return pow(value, n);
  }

  template <>
  inline long double Pow(long double value, long double n, std::false_type)
  {
    return powl(value, n);
  }

  template <typename T>
  inline T Pow(T value, T n)
  {
    return Pow(value, n, std::is_integral<T>());
  }

  template <typename T>
  inline T MinFast(T a, T b)
  {
    return ((a + b) - Abs(a - b)) / static_cast<T> (2);
  }

  template <typename T>
  inline T MaxFast(T a, T b)
  {
    return ((a + b) + Abs(a - b)) / static_cast<T> (2);
  }

  template <typename T>
  inline T ClampFast(T value, T min, T max)
  {
    AssertMessage(min <= max, "Clamp) Invalid Args. min > max");
    return MinFast(max, MinFast(min, value));
  }

  template <typename T>
  inline T Clamp(T value, T min, T max)
  {
    AssertMessage(min <= max, "Clamp) Invalid Args. min > max");
    return std::min(max, std::max(min, value));
  }

  inline float DegreeToRadian(float deg)
  {
    constexpr float inv = 1.0f / 180.0f;
    return deg * Pi * inv;
  }

  inline float RadianToDegree(float rad)
  {
    constexpr float invPi = 1.0f / Pi;
    return rad * 180.0f * invPi;
  }

  inline bool IsZero(float value)
  {
    return Abs(value) < Epsilon;
  }

  inline bool IsUnity(float value)
  {
    return Abs(value - 1.0f) < Epsilon;
  }

  inline bool IsEqual(float a, float b)
  {
    return Abs(a - b) < Epsilon;
  }

  inline bool IsEqual(double a, double b)
  {
	  return Abs(a - b) < Epsilon;
  }

  inline bool IsNotEqual(float a, float b)
  {
    return Abs(a - b) >= Epsilon;
  }

  namespace Physics
  {

    inline float Min(float a, float b)
    {
      return ((a + b) - Abs(a - b)) * 0.5f;
    }

    inline float Max(float a, float b)
    {
      return ((a + b) + Abs(a - b)) * 0.5f;
    }

    inline float Clamp(float value, float min, float max)
    {
      Assert(min <= max);
      return Min(max, Max(min, value));
    }

    inline bool IsZero(float value)
    {
      return Abs(value) < Epsilon;
    }

    inline bool IsEqual(float a, float b)
    {
      return Abs(a - b) < Epsilon;
    }

    inline bool IsNotEqual(float a, float b)
    {
      return Abs(a - b) >= Epsilon;
    }
  }
}


#ifdef __UNIT_TEST__

#include "Test/TestCase.h"

namespace HE
{

  class MathUtilTest : public TestCase
  {
  public:

	  MathUtilTest() : TestCase("MathUtilTest")
    {
    }

  protected:
    virtual bool DoTest() override;
  };
}

#endif //__UNIT_TEST__
