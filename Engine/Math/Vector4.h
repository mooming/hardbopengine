// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "Math/MathUtil.h"
#include "Vector2.h"
#include "Vector3.h"

namespace hbe
{
	/// @brief A 4D vector template class.
	template<typename TNumber>
	class Vector4 final
	{
		using This = Vector4;
		using TVec2 = Vector2<TNumber>;
		using TVec3 = Vector3<TNumber>;

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
				TNumber x;
				TNumber y;
				TNumber z;
				TNumber w;
			};

			TNumber a[order];
		};

	public:
		Vector4() noexcept : This(0, 0, 0, 0) {}

		explicit Vector4(std::nullptr_t) noexcept {}

		Vector4(TNumber x, TNumber y, TNumber z = 0, TNumber w = 1) noexcept : x(x), y(y), z(z), w(w) {}

		explicit Vector4(const TVec2& v, TNumber z = 0, TNumber w = 1) noexcept : x(v.x), y(v.y), z(z), w(w) {}

		explicit Vector4(const TVec3& v, TNumber w = 1) noexcept : x(v.x), y(v.y), z(v.z), w(w) {}

		This& operator=(const TVec2& v) noexcept
		{
			x = v.x;
			y = v.y;

			return *this;
		}

		This& operator=(const TVec3& v) noexcept
		{
			x = v.x;
			y = v.y;
			z = v.z;

			return *this;
		}

		operator TVec2&() noexcept { return reinterpret_cast<TVec2&>(*this); }

		operator const TVec2&() const noexcept { return reinterpret_cast<const TVec2&>(*this); }

		operator TVec3&() noexcept { return reinterpret_cast<TVec3&>(*this); }

		operator const TVec3&() const noexcept { return reinterpret_cast<const TVec3&>(*this); }

#include "VectorCommonImpl.inl"
	};

	template<typename T>
	const Vector4<T> Vector4<T>::Zero(0, 0, 0, 0);
	template<typename T>
	const Vector4<T> Vector4<T>::Unity(1, 1, 1, 1);
	template<typename T>
	const Vector4<T> Vector4<T>::X(1, 0, 0, 0);
	template<typename T>
	const Vector4<T> Vector4<T>::Y(0, 1, 0, 0);
	template<typename T>
	const Vector4<T> Vector4<T>::Z(0, 0, 1, 0);
	template<typename T>
	const Vector4<T> Vector4<T>::W(0, 0, 0, 1);

#ifdef RIGHT_HANDED_COORDINATE
	template<typename T>
	const Vector4<T> Vector4<T>::Right(1.0f, 0.0f, 0.0f);
	template<typename T>
	const Vector4<T> Vector4<T>::Forward(0.0f, 1.0f, 0.0f);
	template<typename T>
	const Vector4<T> Vector4<T>::Up(0.0f, 0.0f, 1.0f);
#endif

#ifndef RIGHT_HANDED_COORDINATE
	template<typename T>
	const Vector4<T> Vector4<T>::Right(1.0f, 0.0f, 0.0f);
	template<typename T>
	const Vector4<T> Vector4<T>::Up(0.0f, 1.0f, 0.0f);
	template<typename T>
	const Vector4<T> Vector4<T>::Forward(0.0f, 0.0f, 1.0f);
#endif

	using TFloat4 = Vector4<float>;

	template<typename TNumber>
	[[nodiscard]] Vector4<TNumber> operator*(TNumber value, Vector4<TNumber> vector) noexcept
	{
		return vector * value;
	}

	template<class TOutStream>
	TOutStream& operator<<(TOutStream& os, const TFloat4& v) noexcept
	{
		os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "), norm = " << v.Length();

		return os;
	}
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

	class Vector4Test final : public TestCollection
	{
	public:
		Vector4Test() : TestCollection("Vector4Test") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
