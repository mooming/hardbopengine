// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include "Core/Types.h"

namespace hbe
{
	/// @brief Deterministic 3D gradient (Perlin) noise with fractal Brownian motion.
	/// @details The permutation table is shuffled by an explicitly specified PRNG
	/// (golden-ratio counter + Murmur3 finalizer) and a hand-written Fisher-Yates pass,
	/// never std::shuffle: the standard leaves std::shuffle's algorithm unspecified, and a
	/// voxel terrain generated from a seed has to come out identical on every platform and
	/// compiler. The gradient function follows Ken Perlin's reference "improved noise".
	///
	/// Two properties are load bearing and pinned by unit tests:
	/// - Noise is exactly zero at every integer lattice coordinate.
	/// - Noise never leaves [-1, 1] (the true bound is about +/-0.87).
	class PerlinNoise final
	{
	public:
		static constexpr TUInt permutationMask = 255;

		explicit PerlinNoise(TUInt seed = 0) noexcept;

		/// @brief Noise value at a point; exactly zero when all coordinates are integers.
		[[nodiscard]] TReal Noise(TReal x, TReal y, TReal z) const noexcept;

		/// @brief Fractal Brownian motion: the sum of successive octaves of Noise.
		/// @details Normalised by the summed amplitude, so the return value stays inside the
		/// range of a single Noise call regardless of how many octaves are requested.
		/// @param octaves Number of octaves; zero returns zero.
		/// @param frequency Frequency of the first octave.
		/// @param lacunarity Frequency multiplier applied per octave.
		/// @param gain Amplitude multiplier applied per octave.
		[[nodiscard]] TReal Fbm(TReal x, TReal y, TReal z, TUInt octaves
			, TReal frequency = 1.0f, TReal lacunarity = 2.0f, TReal gain = 0.5f) const noexcept;

		/// @brief The seed this instance was constructed with.
		[[nodiscard]] TUInt GetSeed() const noexcept { return seed; }

	private:
		/// @brief Double-permutation lookup, already masked and offset.
		[[nodiscard]] TUInt At(TUInt x, TUInt y, TUInt z) const noexcept { return p[p[p[x] + y] + z]; }

		static TReal Grad(TUInt hash, TReal x, TReal y, TReal z) noexcept;
		[[nodiscard]] static TReal Fade(TReal t) noexcept { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
		[[nodiscard]] static TReal Lerp(TReal t, TReal a, TReal b) noexcept { return a + t * (b - a); }

		/// @brief Deterministic replacement for std::shuffle's internals.
		TUInt NextRandom() noexcept;

		TUInt seed = 0;

		/// @brief Separate from seed so GetSeed() stays reportable while shuffling consumes state.
		TUInt randomState = 0;
		std::array<TByte, (permutationMask + 1) * 2> p = {};
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class PerlinNoiseTest final : public TestCollection
	{
	public:
		PerlinNoiseTest() : TestCollection("PerlinNoiseTest") {}

	protected:
		void Prepare() noexcept override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
