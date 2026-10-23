// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>

#include "Core/Types.h"


namespace hbe
{
/// API reference: docs/Math/PerlinNoise/index.html
class PerlinNoise final
{
public:
	static constexpr TUInt permutationMask = 255;

private:
	TUInt seed = 0;

	TUInt randomState = 0;
	std::array<TByte, (permutationMask + 1) * 2> p = {};

public:
	explicit PerlinNoise(TUInt seed = 0) noexcept;

	[[nodiscard]] TReal Noise(TReal x, TReal y, TReal z) const noexcept;

	[[nodiscard]] TReal Fbm(TReal x, TReal y, TReal z, TUInt octaves, TReal frequency = 1.0f, TReal lacunarity = 2.0f,
							TReal gain = 0.5f) const noexcept;

	[[nodiscard]] TUInt GetSeed() const noexcept
	{
		return seed;
	}

private:
	static TReal Grad(TUInt hash, TReal x, TReal y, TReal z) noexcept;

	[[nodiscard]] static TReal Fade(TReal t) noexcept
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	[[nodiscard]] static TReal Lerp(TReal t, TReal a, TReal b) noexcept
	{
		return a + t * (b - a);
	}

	[[nodiscard]] TUInt At(TUInt x, TUInt y, TUInt z) const noexcept
	{
		return p[p[p[x] + y] + z];
	}

	TUInt NextRandom() noexcept;
};
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class PerlinNoiseTest final : public TestCollection
{
public:
	PerlinNoiseTest()
		: TestCollection("PerlinNoiseTest")
	{
	}

protected:
	void Prepare() noexcept override;
};
} // namespace hbe
#endif // TEST_ENABLED
