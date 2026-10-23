// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <random>
#include <type_traits>

#include "Core/CommonMacros.h"
#include "HSTL/HVector.h"


namespace hbe
{
template <typename TInput = double, typename TOutput = double, typename TReal = double, typename TInteger = uint32_t>
/// API reference: docs/Math/ImportanceResampling/index.html
class ImportanceResampling final
{
public:
	static_assert(std::is_floating_point_v<TReal>);
	static_assert(std::is_integral_v<TInteger>);

private:
	TReal totalWeight;
	hbe::HVector<TReal> weights;
	hbe::HVector<TInput> samples;
	hbe::HVector<TReal> normalizedWeights;

public:
	ImportanceResampling() = default;
	~ImportanceResampling() = default;

	[[nodiscard]] auto& GetWeights() const noexcept
	{
		return weights;
	}

	[[nodiscard]] auto& GetSamples() const noexcept
	{
		return samples;
	}

	[[nodiscard]] auto& GetNormalizedWeights() const noexcept
	{
		return normalizedWeights;
	}

	void Reset() noexcept
	{
		totalWeight = 0;
		std::swap(weights, hbe::HVector<TReal>());
		std::swap(normalizedWeights, hbe::HVector<TReal>());
		std::swap(samples, hbe::HVector<TInput>());
	}

	void ClearResampledData() noexcept
	{
		totalWeight = 0;
		weights.clear();
		samples.clear();
		normalizedWeights.clear();
	}

	template <typename TFunction, typename TPDF, typename TRandomSampler, typename TOutputNorm>
	[[nodiscard]] bool Resample(const TFunction& f, const TPDF& p, const TRandomSampler& sampler,
								const TOutputNorm& norm, const TInteger numSourceSamples) noexcept
	{
		returnValueIf(false, numSourceSamples <= 0);

		weights.reserve(weights.size() + numSourceSamples);
		samples.reserve(samples.size() + numSourceSamples);

		TReal sumWeights = 0;
		for (TInteger i = 0; i < numSourceSamples; ++i)
		{
			const TInput x = sampler();
			const TReal p_x = p(x);
			returnValueIf(false, p_x <= 0);

			const TOutput f_x = f(x);
			const TOutput fx_over_px = f_x / p_x;
			const TReal weight = norm(fx_over_px);

			continueIf(weight <= 0);

			sumWeights += weight;
			weights.emplace_back(weight);
			samples.emplace_back(x);
		}

		if (sumWeights <= 0)
			return false;

		totalWeight += sumWeights;
		{
			const auto numSamples = weights.size();
			normalizedWeights.clear();
			normalizedWeights.reserve(numSamples);

			const TReal normalizeFactor = static_cast<TReal>(numSamples) / totalWeight;
			for (auto& weight : weights)
			{
				normalizedWeights.push_back(weight * normalizeFactor);
			}
		}

		return true;
	}

	template <typename TFunction, typename TDiscreteSampler, typename TUniformSampler>
	[[nodiscard]] bool Integrate(TOutput& result, const TFunction& f, const TDiscreteSampler& discreteSampler,
								 const TUniformSampler& uniformSampler, const TInteger numIterations) noexcept
	{
		result = 0;

		returnValueIf(false, numIterations <= 0 || f == nullptr);
		returnValueIf(false, samples.empty() || weights.empty());

		for (TInteger i = 0; i < numIterations; ++i)
		{
			const auto chosenIndex = discreteSampler();
			const TReal q_x = normalizedWeights[chosenIndex];
			if (q_x <= 0)
			{
				return false;
			}

			const auto delta = uniformSampler();
			auto x = samples[chosenIndex];
			x += delta;
			const auto f_x = f(x);
			const TOutput fx_over_qx = f_x / q_x;
			result += fx_over_qx;
		}

		result /= numIterations;

		return true;
	}
};
} // namespace hbe

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class ImportanceResamplingTest final : public TestCollection
{
public:
	ImportanceResamplingTest()
		: TestCollection("Importance Resampling Test")
	{
	}

protected:
	void Prepare() noexcept override;
};
} // namespace hbe
#endif // TEST_ENABLED
