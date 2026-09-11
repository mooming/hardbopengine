// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <limits>
#include <ostream>
#include "MathUtil.h"
#include "String/StringBuilder.h"
#include "Vector2.h"
#include "Vector3.h"

namespace hbe
{
	/// @brief An Axis-Aligned Bounding Box template class.
	template<typename TVec>
	class AABB final
	{
		using This = AABB;
		static constexpr auto MAX = std::numeric_limits<float>::max();

	public:
		TVec min;
		TVec max;

	public:
		explicit AABB(std::nullptr_t) noexcept {}

		AABB() noexcept : min(TVec::Unity * MAX), max(TVec::Unity * -MAX) {}

		AABB(const TVec& min, const TVec& max) noexcept : min(min), max(max) {}

		void reset() noexcept
		{
			min = TVec::Unity * MAX;
			max = -TVec::Unity * MAX;
		}

		[[nodiscard]] This operator+(const TVec& rhs) const noexcept
		{
			auto result = *this;
			result += rhs;

			return result;
		}

		[[nodiscard]] This operator+(const This& rhs) const noexcept
		{
			auto result = *this;
			result += rhs;

			return result;
		}

		void operator+=(const TVec& rhs) noexcept { add(rhs); }

		void operator+=(const This& rhs) noexcept { add(rhs); }

		[[nodiscard]] bool operator==(const This& rhs) const noexcept
		{
			return isContaining(rhs) && rhs.isContaining(*this);
		}

		[[nodiscard]] bool operator!=(const This& rhs) const noexcept { return !(*this == rhs); }

		void add(const TVec& point) noexcept
		{
			for (int i = 0; i < TVec::order; ++i)
			{
				min.a[i] = minFast(point.a[i], min.a[i]);
				max.a[i] = maxFast(point.a[i], max.a[i]);
			}
		}

		void add(const This& aabb) noexcept
		{
			add(aabb.min);
			add(aabb.max);
		}

		void translate(const TVec& t) noexcept
		{
			Assert(!IsEmpty(), "Do not translate an empty AABB! ", *this);
			min += t;
			max += t;
		}

		[[nodiscard]] bool IsEmpty() const noexcept
		{
			for (int i = 0; i < TVec::order; ++i)
			{
				if (max.a[i] <= min.a[i])
				{
					return true;
				}
			}

			return false;
		}

		[[nodiscard]] bool isContaining(const TVec& point) const noexcept
		{
			if (IsEmpty())
			{
				return false;
			}

			for (int i = 0; i < TVec::order; ++i)
			{
				if (min.a[i] > point.a[i])
				{
					return false;
				}

				if (max.a[i] < point.a[i])
				{
					return false;
				}
			}

			return true;
		}

		[[nodiscard]] bool isContaining(const This& aabb) const noexcept
		{
			return isContaining(aabb.min) && isContaining(aabb.max);
		}

		[[nodiscard]] AABB intersection(const AABB& aabb) const noexcept
		{
			This result(nullptr);

			for (int i = 0; i < TVec::order; ++i)
			{
				result.min.a[i] = maxFast(aabb.min.a[i], min.a[i]);
				result.max.a[i] = minFast(aabb.max.a[i], max.a[i]);
			}

			return result;
		}

		[[nodiscard]] bool hasIntersectionWith(const AABB& aabb) const noexcept
		{
			return !intersection(aabb).IsEmpty();
		}

		[[nodiscard]] TVec center() const noexcept { return (min + max) * 0.5f; }

		[[nodiscard]] TVec diagonal() const noexcept { return max - min; }

		[[nodiscard]] TVec half() const noexcept { return diagonal() * 0.5f; }

		[[nodiscard]] TVec closest(const TVec& point) const noexcept
		{
			TVec closePt = point;

			for (int i = 0; i < TVec::order; ++i)
			{
				closePt.a[i] = clampFast(point.a[i], min.a[i], max.a[i]);
			}

			return closePt;
		}
	};

	using AABB2 = AABB<TFloat2>;
	using AABB3 = AABB<TFloat3>;

	template<typename T>
	std::ostream& operator<<(std::ostream& os, const AABB<T>& bbox) noexcept
	{
		using std::endl;

		os << "AABB min = " << bbox.min << ", max = " << bbox.max;

		return os;
	}

	template<class TStringBuilder, typename T>
	TStringBuilder& operator<<(TStringBuilder& os, const AABB<T>& bbox) noexcept
	{
		os << "AABB min = " << bbox.min << ", max = " << bbox.max;

		return os;
	}

} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class AABBTest final : public TestCollection
	{
	public:
		AABBTest() : TestCollection("AABBTest") {}

	protected:
		void prepare() noexcept override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
