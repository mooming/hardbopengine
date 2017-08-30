// Copyright, all rights reserved by Hansol Park, mooming.go@gamil.com

#ifndef AABB_H

#include "Vector2.h"
#include "Vector3.h"

#include "../System/CommonUtil.h"

namespace HE
{

  template <typename Vec>
  class AABB
  {
    using This = AABB;

  public:
    Vec min;
    Vec max;

  public:

    inline AABB(std::nullptr_t)
    {
    }

    inline AABB() : min(), max()
    {
    }

    inline AABB(const Vec& min, const Vec& max) : min(min), max(max)
    {
    }

    void Reset()
    {
      min = Vec::Zero;
      max = Vec::Zero;
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
  };

  using AABB2 = AABB<Float2>;
  using AABB3 = AABB<Float3>;
}

#endif  // AABB_H
