// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Vector2.h"
#include "Vector3.h"

#include "../System/MathUtil.h"

#include <limits>
#include <ostream>

namespace HE
{

  template <typename Vec>
  class AABB
  {
    using This = AABB;
    static constexpr auto MAX = std::numeric_limits<float>::max();

  public:
    Vec min;
    Vec max;

  public:

    inline AABB(std::nullptr_t)
    {
    }

    inline AABB() : min(Vec::Unity * MAX), max(Vec::Unity * -MAX)
    {
    }

    inline AABB(const Vec& min, const Vec& max) : min(min), max(max)
    {
    }

    void Reset()
    {
      min = Vec::Unity * MAX;
      max = -Vec::Unity * MAX;
    }

    This operator+(const Vec& rhs) const
    {
      auto result = *this;
      result += rhs;

      return result;
    }

    This operator+(const This& rhs) const
    {
      auto result = *this;
      result += rhs;

      return result;
    }

    void operator+=(const Vec& rhs)
    {
      Add(rhs);
    }

    void operator+=(const This& rhs)
    {
      Add(rhs);
    }

    bool operator==(const This& rhs) const
    {

      return IsContaining(rhs) && rhs.IsContaining(*this);
    }

    bool operator!=(const This& rhs) const
    {
      return !(*this == rhs);
    }

    inline void Add(const Vec& point)
    {
      auto length = Vec::order;
      for (int i = 0; i < length; ++i)
      {
        min.a[i] = MinFast(point.a[i], min.a[i]);
        max.a[i] = MaxFast(point.a[i], max.a[i]);
      }
    }

    inline void Add(const This& aabb)
    {
      Add(aabb.min);
      Add(aabb.max);
    }

    inline void Translate(const Vec& t)
    {
      AssertMessage(!IsEmpty(), "Do not translate an empty AABB! ", *this);
      min += t;
      max += t;
    }

    bool IsEmpty() const
    {
      auto length = Vec::order;
      for (int i = 0; i < length; ++i)
      {
        if (max.a[i] <= min.a[i])
          return true;
      }

      return false;
    }

    inline bool IsContaining(const Vec& point) const
    {
      if (IsEmpty())
        return false;

      auto length = Vec::order;
      for (int i = 0; i < length; ++i)
      {
        if (min.a[i] > point.a[i])
          return false;

        if (max.a[i] < point.a[i])
          return false;
      }

      return true;
    }

    inline bool IsContaining(const This& aabb) const
    {
      return IsContaining(aabb.min) && IsContaining(aabb.max);
    }

    inline AABB Intersection(const AABB& aabb) const
    {
      This result = nullptr;

      auto length = Vec::order;
      for (int i = 0; i < length; ++i)
      {
        result.min.a[i] = MaxFast(aabb.min.a[i], min.a[i]);
        result.max.a[i] = MinFast(aabb.max.a[i], max.a[i]);
      }

      return result;
    }

    inline bool HasIntersectionWith(const AABB& aabb) const
    {
      return !Intersection(aabb).IsEmpty();
    }

    inline Vec Center() const
    {
      return (min + max) * 0.5f;
    }

    inline Vec Diagonal() const
    {
      return max - min;
    }

    inline Vec Half() const
    {
      return Diagonal() * 0.5f;
    }

	inline Vec Closest(const Vec& point) const
	{
		Vec closePt = point;

		auto length = Vec::order;
		for (int i = 0; i < length; ++i)
		{
			closePt.a[i] = ClampFast(point.a[i], min.a[i], max.a[i]);
		}

		return closePt;
	}
  };

  using AABB2 = AABB<Float2>;
  using AABB3 = AABB<Float3>;

  template <typename T>
  inline std::ostream& operator<<(std::ostream& os, const AABB<T>& bbox)
  {
    using std::endl;

    os << "AABB min = " << bbox.min << ", max = " << bbox.max;

    return os;
  }
}

#ifdef __UNIT_TEST__

#include "System/TestCase.h"

namespace HE
{

  class AABBTest : public TestCase
  {
  public:

    AABBTest() : TestCase("AABBTest")
    {
    }

  protected:
    virtual bool DoTest() override;
  };
}
#endif //__UNIT_TEST__
