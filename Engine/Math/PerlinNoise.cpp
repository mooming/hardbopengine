// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "PerlinNoise.h"

#include <algorithm>
#include <cmath>

namespace hbe
{

PerlinNoise::PerlinNoise(TUInt inSeed) noexcept
	: seed(inSeed)
	, randomState(inSeed)
{
	for (TUInt i = 0; i <= permutationMask; ++i)
	{
		p[i] = static_cast<TByte>(i);
	}

	// Fisher-Yates over the low half; At() indexes the doubled table, so only the first
	// 256 entries need to be a permutation.
	for (TUInt i = permutationMask; i > 0; --i)
	{
		const TUInt j = NextRandom() % (i + 1);
		const TByte swap = p[i];
		p[i] = p[j];
		p[j] = swap;
	}

	for (TUInt i = 0; i <= permutationMask; ++i)
	{
		p[i + permutationMask + 1] = p[i];
	}
}

TReal PerlinNoise::Noise(TReal x, TReal y, TReal z) const noexcept
{
	const TReal cellX = std::floor(x);
	const TReal cellY = std::floor(y);
	const TReal cellZ = std::floor(z);

	// Cast before masking so negative coordinates wrap into the table instead of
	// truncating toward zero and colliding with their positive mirror.
	const TUInt X = static_cast<TUInt>(static_cast<TInt>(cellX)) & permutationMask;
	const TUInt Y = static_cast<TUInt>(static_cast<TInt>(cellY)) & permutationMask;
	const TUInt Z = static_cast<TUInt>(static_cast<TInt>(cellZ)) & permutationMask;

	const TReal fx = x - cellX;
	const TReal fy = y - cellY;
	const TReal fz = z - cellZ;

	const TReal u = Fade(fx);
	const TReal v = Fade(fy);
	const TReal w = Fade(fz);

	const TReal n000 = Grad(At(X    , Y    , Z    ), fx      , fy      , fz      );
	const TReal n100 = Grad(At(X + 1, Y    , Z    ), fx - 1.0, fy      , fz      );
	const TReal n010 = Grad(At(X    , Y + 1, Z    ), fx      , fy - 1.0, fz      );
	const TReal n110 = Grad(At(X + 1, Y + 1, Z    ), fx - 1.0, fy - 1.0, fz      );
	const TReal n001 = Grad(At(X    , Y    , Z + 1), fx      , fy      , fz - 1.0);
	const TReal n101 = Grad(At(X + 1, Y    , Z + 1), fx - 1.0, fy      , fz - 1.0);
	const TReal n011 = Grad(At(X    , Y + 1, Z + 1), fx      , fy - 1.0, fz - 1.0);
	const TReal n111 = Grad(At(X + 1, Y + 1, Z + 1), fx - 1.0, fy - 1.0, fz - 1.0);

	const TReal y0z0 = Lerp(u, n000, n100);
	const TReal y1z0 = Lerp(u, n010, n110);
	const TReal y0z1 = Lerp(u, n001, n101);
	const TReal y1z1 = Lerp(u, n011, n111);

	return Lerp(w, Lerp(v, y0z0, y1z0), Lerp(v, y0z1, y1z1));
}

TReal PerlinNoise::Fbm(TReal x, TReal y, TReal z, TUInt octaves
	, TReal frequency, TReal lacunarity, TReal gain) const noexcept
{
	TReal sum = 0.0f;
	TReal amplitude = 1.0f;
	TReal normalisation = 0.0f;
	TReal freq = frequency;

	for (TUInt octave = 0; octave < octaves; ++octave)
	{
		sum += amplitude * Noise(x * freq, y * freq, z * freq);
		normalisation += amplitude;
		freq *= lacunarity;
		amplitude *= gain;
	}

	return normalisation > 0.0f ? sum / normalisation : 0.0f;
}

TUInt PerlinNoise::NextRandom() noexcept
{
	// Golden-ratio counter plus the Murmur3 finalizer. Everything here is specified by
	// arithmetic rather than by a library implementation detail, which is the whole point:
	// the shuffle must be identical on every platform.
	randomState += 0x9E3779B9u;

	TUInt mixed = randomState;
	mixed = (mixed ^ (mixed >> 16)) * 0x85EBCA6Bu;
	mixed = (mixed ^ (mixed >> 13)) * 0xC2B2AE35u;

	return mixed ^ (mixed >> 16);
}

TReal PerlinNoise::Grad(TUInt hash, TReal x, TReal y, TReal z) noexcept
{
	// Ken Perlin's reference gradient selection for "improved noise".
	switch (hash & 15)
	{
		case 0x0: return x + y;
		case 0x1: return -x + y;
		case 0x2: return x - y;
		case 0x3: return -x - y;
		case 0x4: return x + z;
		case 0x5: return -x + z;
		case 0x6: return x - z;
		case 0x7: return -x - z;
		case 0x8: return y + z;
		case 0x9: return -y + z;
		case 0xA: return y - z;
		case 0xB: return -y - z;
		case 0xC: return y + x;
		case 0xD: return -y + z;
		case 0xE: return y - x;
		case 0xF: return -y - z;
		default: return 0.0f;
	}
}

} // namespace hbe

#ifdef __UNIT_TEST__

// Note on the flush convention: TestCollection's operator<< sends whatever the test stream
// holds so far to the logger, at the level of the LogFlush handed to it (lf = Info,
// lferr = Error). It does not clear the stream, so each test flushes exactly once - at Error
// level when a check failed, since that carries the measured numbers along with the reason.
void hbe::PerlinNoiseTest::Prepare() noexcept
{
	AddTest("Noise Determinism", [this](auto& ls)
	{
		PerlinNoise a(12345);
		PerlinNoise b(12345);
		PerlinNoise c(999);

		TInt sameSeed = 0;
		TInt otherSeed = 0;
		TInt samples = 0;

		for (int i = 0; i < 16; ++i)
		{
			for (int j = 0; j < 16; ++j)
			{
				const float x = static_cast<float>(i) * 0.37f - 2.5f;
				const float y = static_cast<float>(j) * 0.41f + 1.25f;
				const float z = static_cast<float>(i + j) * 0.13f;

				if (a.Noise(x, y, z) != b.Noise(x, y, z))
				{
					++sameSeed;
				}
				if (a.Noise(x, y, z) != c.Noise(x, y, z))
				{
					++otherSeed;
				}
				++samples;
			}
		}

		ls << samples << " samples, same-seed mismatches: " << sameSeed
		   << ", different-seed mismatches: " << otherSeed;

		if (sameSeed != 0)
		{
			ls << " | FAIL: the same seed produced a different field" << lferr;
		}
		else if (otherSeed == 0)
		{
			ls << " | FAIL: the seed is not used to build the permutation at all" << lferr;
		}
		else
		{
			ls << lf;
		}
	});

	AddTest("Lattice Zeros", [this](auto& ls)
	{
		PerlinNoise noise(7);
		TInt nonZero = 0;
		float worst = 0.0f;

		for (int i = -4; i <= 4; ++i)
		{
			for (int j = -4; j <= 4; ++j)
			{
				for (int k = -4; k <= 4; ++k)
				{
					const float value = noise.Noise(static_cast<float>(i), static_cast<float>(j)
						, static_cast<float>(k));
					if (value != 0.0f)
					{
						++nonZero;
						worst = std::max(worst, std::abs(value));
					}
				}
			}
		}

		ls << "729 lattice points, non-zero: " << nonZero << ", largest |value|: " << worst;

		if (nonZero != 0)
		{
			ls << " | FAIL: " << nonZero << " lattice points were not exactly zero (worst "
			   << worst << "), so the fade/lerp blend is leaking neighbouring corners" << lferr;
		}
		else
		{
			ls << lf;
		}
	});

	AddTest("Range Bounds", [this](auto& ls)
	{
		PerlinNoise noise(2024);
		float largest = 0.0f;
		TInt nonFinite = 0;
		TInt samples = 0;

		for (int i = 0; i < 40; ++i)
		{
			for (int j = 0; j < 40; ++j)
			{
				// Negative coordinates and a coordinate past the table period are table
				// indexing traps rather than math problems, so they are sampled on purpose.
				const float x = static_cast<float>(i) * 0.61f - 12.0f;
				const float y = static_cast<float>(j) * 0.17f - 3.0f;
				const float z = 257.5f + static_cast<float>(i - j) * 0.05f;

				const float basic = noise.Noise(x, y, z);
				const float fbm = noise.Fbm(x, y, z, 5);

				for (const float value : { basic, fbm })
				{
					++samples;
					if (!std::isfinite(value))
					{
						++nonFinite;
					}
					largest = std::max(largest, std::abs(value));
				}
			}
		}

		ls << samples << " samples, largest |value|: " << largest << ", non-finite: " << nonFinite;

		if (nonFinite != 0)
		{
			ls << " | FAIL: " << nonFinite << " non-finite samples" << lferr;
		}
		else if (largest > 1.0f)
		{
			ls << " | FAIL: |value| exceeded the documented [-1, 1] bound" << lferr;
		}
		else if (largest < 0.05f)
		{
			ls << " | FAIL: noise is effectively flat" << lferr;
		}
		else
		{
			ls << lf;
		}
	});

	AddTest("Continuity", [this](auto& ls)
	{
		PerlinNoise noise(31);
		float largestStep = 0.0f;
		const float step = 0.02f;
		float previous = noise.Noise(0.1f, 0.7f, 0.3f);

		for (int i = 1; i < 2000; ++i)
		{
			const float x = 0.1f + static_cast<float>(i) * step;
			const float current = noise.Noise(x, 0.7f, 0.3f);
			largestStep = std::max(largestStep, std::abs(current - previous));
			previous = current;
		}

		ls << "1999 steps of " << step << ", largest single-step jump: " << largestStep;

		// Perlin is C1 continuous, so a small step must give a proportionally small change.
		// A discontinuity - a wrong fade, or a seam at the table period - shows up as a jump
		// comparable to the size of the signal itself.
		if (largestStep > 0.1f)
		{
			ls << " | FAIL: a jump this large at this step size indicates a discontinuity" << lferr;
		}
		else
		{
			ls << lf;
		}
	});

	AddTest("Fbm Normalisation", [this](auto& ls)
	{
		PerlinNoise noise(88);
		TInt failures = 0;
		float largest = 0.0f;

		for (TUInt octaves = 1; octaves <= 6; ++octaves)
		{
			for (int i = 0; i < 20; ++i)
			{
				const float x = static_cast<float>(i) * 0.44f - 4.0f;
				const float value = noise.Fbm(x, 1.5f, -2.25f, octaves, 0.8f);
				largest = std::max(largest, std::abs(value));
				if (!std::isfinite(value) || std::abs(value) > 1.0f)
				{
					++failures;
				}
			}
		}

		const float zeroctaves = noise.Fbm(1.25f, 2.5f, 3.75f, 0);

		ls << "octaves 1..6, largest |value|: " << largest << ", out of range: " << failures
		   << ", octaves=0 -> " << zeroctaves;

		if (failures != 0)
		{
			ls << " | FAIL: fBm escaped the single-octave range, the amplitude normalisation "
			   << "is wrong" << lferr;
		}
		else if (zeroctaves != 0.0f)
		{
			ls << " | FAIL: zero octaves should return zero" << lferr;
		}
		else
		{
			ls << lf;
		}
	});
}

#endif //__UNIT_TEST__
