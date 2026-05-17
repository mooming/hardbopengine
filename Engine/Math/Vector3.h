// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cmath>
#include "Config/BuildConfig.h"
#include "Math/MathUtil.h"
#include "Vector2.h"

namespace hbe
{
	/// @brief A 3D vector template class.
	template<typename TNumber>
	class Vector3 final
	{
		using This = Vector3;
		using TVec2 = Vector2<TNumber>;

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
				TNumber x;
				TNumber y;
				TNumber z;
			};

			TNumber a[order];
		};

	public:
		Vector3() noexcept : This(0, 0, 0) {}

		explicit Vector3(std::nullptr_t) noexcept {}

		Vector3(TNumber x, TNumber y) noexcept : This(x, y, 0) {}

		Vector3(TNumber x, TNumber y, TNumber z) noexcept : x(x), y(y), z(z) {}

		explicit Vector3(const TVec2& v, TNumber z = 0) noexcept : x(v.x), y(v.y), z(z) {}

		This& operator=(const TVec2& v) noexcept
		{
			x = v.x;
			y = v.y;

			return *this;
		}

		explicit operator TVec2&() noexcept { return reinterpret_cast<TVec2&>(*this); }

		explicit operator const TVec2&() const noexcept { return reinterpret_cast<const TVec2&>(*this); }

#include "VectorCommonImpl.inl"

	public:
		[[nodiscard]] Vector3 Cross(const Vector3& rhs) const noexcept
		{
			Vector3 result(nullptr);

			result.x = y * rhs.z - z * rhs.y;
			result.y = z * rhs.x - x * rhs.z;
			result.z = x * rhs.y - y * rhs.x;

			return result;
		}

		[[nodiscard]] float AngleTo(const Vector3& to) const noexcept
		{
			Assert(!IsZero());
			Assert(!to.IsZero());
			float r = sqrtf(static_cast<float>(SqrLength() * to.SqrLength()));

			return std::acos(static_cast<float>(Dot(to)) / r);
		}
	};

	template<typename T>
	const Vector3<T> Vector3<T>::Zero(0, 0, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Unity(1, 1, 1);
	template<typename T>
	const Vector3<T> Vector3<T>::X(1, 0, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Y(0, 1, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Z(0, 0, 1);

#ifdef RIGHT_HANDED_COORDINATE
	template<typename T>
	const Vector3<T> Vector3<T>::Right(1, 0, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Forward(0, 1, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Up(0, 0, 1);
#endif //RIGHT_HANDED_COORDINATE

#ifndef RIGHT_HANDED_COORDINATE
	template<typename T>
	const Vector3<T> Vector3<T>::Right(1, 0, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Up(0, 1, 0);
	template<typename T>
	const Vector3<T> Vector3<T>::Forward(0, 0, 1);
#endif

	using TInt3 = Vector3<int>;
	using TFloat3 = Vector3<float>;

	template<typename TNumber>
	[[nodiscard]] Vector3<TNumber> operator*(TNumber value, Vector3<TNumber> vector) noexcept
	{
		return vector * value;
	}

	template<class TOutStream, typename TNumber>
	TOutStream& operator<<(TOutStream& os, const Vector3<TNumber>& v) noexcept
	{
		os << "(" << v.x << ", " << v.y << ", " << v.z << "), norm = " << v.Length();

		return os;
	}
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class Vector3Test final : public TestCollection
	{
	public:
		Vector3Test() : TestCollection("Vector3Test") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
