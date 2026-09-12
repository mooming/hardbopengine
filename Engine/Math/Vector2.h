// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "MathUtil.h"

namespace hbe
{
	/// @brief A 2D vector template class.
	template<typename TNumber>
	class Vector2 final
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
				TNumber x;
				TNumber y;
			};

			TNumber a[order];
		};

	public:
		Vector2() noexcept : This(0, 0) {}

		Vector2(TNumber x, TNumber y) noexcept : x(x), y(y) {}

		explicit Vector2(std::nullptr_t) noexcept {}

#include "VectorCommonImpl.inl"
	};

	template<typename T>
	const Vector2<T> Vector2<T>::Zero(0, 0);
	template<typename T>
	const Vector2<T> Vector2<T>::Unity(1, 1);
	template<typename T>
	const Vector2<T> Vector2<T>::X(1, 0);
	template<typename T>
	const Vector2<T> Vector2<T>::Y(0, 1);
	template<typename T>
	const Vector2<T> Vector2<T>::Forward(1, 0);
	template<typename T>
	const Vector2<T> Vector2<T>::Up(0, 1);

	using TInt2 = Vector2<int>;
	using TFloat2 = Vector2<float>;

	template<typename T>
	[[nodiscard]] Vector2<T> operator*(T value, Vector2<T> vector) noexcept
	{
		return vector * value;
	}

	template<class TOutStream>
	TOutStream& operator<<(TOutStream& os, const TFloat2& vec) noexcept
	{
		os << "(" << vec.x << ", " << vec.y << "), norm = " << vec.Length();

		return os;
	}
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class Vector2Test final : public TestCollection
	{
	public:
		Vector2Test() : TestCollection("Vector2Test") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe

#endif //__UNIT_TEST__
