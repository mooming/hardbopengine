// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cmath>
#include <cstdint>
#include "Core/Constants.h"
#include "Core/Debug.h"

namespace hbe
{

	template<typename T>
	[[nodiscard]] T Abs(T value, std::false_type) noexcept
	{
		return std::abs(value);
	}

	template<typename T>
	[[nodiscard]] T Abs(T value, std::true_type) noexcept
	{
		constexpr int shift = sizeof(T) * 8 - 1;
		const uint8_t mask = value >> shift;

		return (value + mask) ^ mask;
	}

	template<typename T>
	[[nodiscard]] T Abs(T value) noexcept
	{
		return Abs<T>(value, std::is_integral<T>());
	}

	template<typename T>
	[[nodiscard]] T Pow(T value, T n, std::true_type) noexcept
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

	template<typename T>
	T Pow(T value, T n, std::false_type) noexcept;

	template<>
	[[nodiscard]] inline float Pow(float value, float n, std::false_type) noexcept
	{
		return powf(value, n);
	}

	template<>
	[[nodiscard]] inline double Pow(double value, double n, std::false_type) noexcept
	{
		return pow(value, n);
	}

	template<>
	[[nodiscard]] inline long double Pow(long double value, long double n, std::false_type) noexcept
	{
		return powl(value, n);
	}

	template<typename T>
	[[nodiscard]] T Pow(T value, T n) noexcept
	{
		return Pow(value, n, std::is_integral<T>());
	}

	template<typename T>
	[[nodiscard]] T MinFast(T a, T b) noexcept
	{
		return ((a + b) - Abs(a - b)) / static_cast<T>(2);
	}

	template<typename T>
	[[nodiscard]] T MaxFast(T a, T b) noexcept
	{
		return ((a + b) + Abs(a - b)) / static_cast<T>(2);
	}

	template<typename T>
	[[nodiscard]] T ClampFast(T value, T min, T max) noexcept
	{
		Assert(min <= max, "Clamp) Invalid Args. min > max");

		return MinFast(max, MinFast(min, value));
	}

	template<typename T>
	[[nodiscard]] T Clamp(T value, T min, T max) noexcept
	{
		Assert(min <= max, "Clamp) Invalid Args. min > max");

		return std::min(max, std::max(min, value));
	}

	[[nodiscard]] inline float DegreeToRadian(float deg) noexcept
	{
		constexpr float inv = 1.0f / 180.0f;

		return deg * Pi * inv;
	}

	[[nodiscard]] inline float RadianToDegree(float rad) noexcept
	{
		constexpr float invPi = 1.0f / Pi;

		return rad * 180.0f * invPi;
	}

	[[nodiscard]] inline bool IsZero(float value) noexcept { return Abs(value) < Epsilon; }

	[[nodiscard]] inline bool IsUnity(float value) noexcept { return Abs(value - 1.0f) < Epsilon; }

	[[nodiscard]] inline bool IsEqual(float a, float b) noexcept { return Abs(a - b) < Epsilon; }

	[[nodiscard]] inline bool IsEqual(double a, double b) noexcept { return Abs(a - b) < Epsilon; }

	[[nodiscard]] inline bool IsNotEqual(float a, float b) noexcept { return Abs(a - b) >= Epsilon; }

	namespace Physics
	{

		[[nodiscard]] inline float Min(float a, float b) noexcept { return ((a + b) - Abs(a - b)) * 0.5f; }

		[[nodiscard]] inline float Max(float a, float b) noexcept { return ((a + b) + Abs(a - b)) * 0.5f; }

		[[nodiscard]] inline float Clamp(float value, float min, float max) noexcept
		{
			Assert(min <= max);

			return Min(max, Max(min, value));
		}

		[[nodiscard]] inline bool IsZero(float value) noexcept { return Abs(value) < Epsilon; }

		[[nodiscard]] inline bool IsEqual(float a, float b) noexcept { return Abs(a - b) < Epsilon; }

		[[nodiscard]] inline bool IsNotEqual(float a, float b) noexcept { return Abs(a - b) >= Epsilon; }

	} // namespace Physics

} // namespace hbe

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"

namespace hbe
{

	class MathUtilTest final : public TestCollection
	{
	public:
		MathUtilTest() : TestCollection("MathUtilTest") {}

	protected:
		void Prepare() noexcept override;
	};

} // namespace hbe

#endif //__UNIT_TEST__
