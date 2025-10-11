//
// Created by mooming on 22/06/2025.
//

#pragma once

#include "HSTL/HVector.h"

#include <random>
#include <type_traits>

namespace hbe
{
	template<typename TInput = double, typename TOutput = double, typename TReal = double, typename TInteger = uint32_t>
	class ImportanceResampling
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

		auto& GetWeights() const noexcept { return weights; }
		auto& GetSamples() const noexcept { return samples; }
		auto& GetNormalizedWeights() const noexcept { return normalizedWeights; }

		void Reset()
		{
			totalWeight = 0;
			std::swap(weights, hbe::HVector<TReal>());
			std::swap(normalizedWeights, hbe::HVector<TReal>());
			std::swap(samples, hbe::HVector<TInput>());
		}

		void ClearResampledData()
		{
			totalWeight = 0;
			weights.clear();
			samples.clear();
			normalizedWeights.clear();
		}

		///
		/// Resample Inportance Sampling
		///
		///  To utilise good samples more, it'll resample by weights
		///
		/// @param f A function that you want to integrate, TOutput f(const TInput& input)
		/// @param p A source probability density function, TReal p(const TInput& input)
		/// @param sampler Random input generator, TInput sampler()
		/// @param numSourceSamples Number of source samples
		/// @param norm Calculate a norm of the given function output, TReal norm(const TOutput& output)
		/// @return false if it fails to resample
		///
		template<typename TFunction, typename TPDF, typename TRandomSampler, typename TOutputNorm>
		bool Resample(const TFunction& f, const TPDF& p, const TRandomSampler& sampler, const TOutputNorm& norm,
					  const TInteger numSourceSamples)
		{
			if (numSourceSamples <= 0)
			{
				return false;
			}

			weights.reserve(weights.size() + numSourceSamples);
			samples.reserve(samples.size() + numSourceSamples);

			TReal sumWeights = 0;
			for (TInteger i = 0; i < numSourceSamples; ++i)
			{
				const TInput x = sampler();
				const TReal p_x = p(x);
				if (p_x <= 0)
				{
					return false;
				}

				const TOutput f_x = f(x);
				const TOutput fx_over_px = f_x / p_x;
				const TReal weight = norm(fx_over_px);

				if (weight <= 0)
				{
					continue;
				}

				sumWeights += weight;
				weights.emplace_back(weight);
				samples.emplace_back(x);
			}

			if (sumWeights <= 0)
			{
				return false;
			}

			totalWeight += sumWeights;

			// Normalize Discrete PDF
			// stepWidth = 1 / numSourceSamples
			// Total Area = Sum (W[i] * stepWidth) = stepWidth * Sum(W[i]) = StepWith * totalWeight = totalWeight /
			// numSourceSamples normalizeFactor = numSourceSamples / totalWeight
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

		///
		/// Monte Carlo Method with Importance resampled importance sampling
		///
		///  Importance resampling is a sample generation technique that can be used to generate more equally weighted
		///  samples for importance sampling.
		///
		///
		/// @param result result of the integration
		/// @param f A function that you want to integrate, TOutput f(const TInput& input)
		/// @param discreteSampler A random sampler for index based on resampled probability, TInteger sampler()
		/// @param uniformSampler A random sampler for input value random deviation, TInput sampler()
		/// @param numIterations Number of iterations with resampled importance sampling
		/// @return false if it fails to calculate the integration.
		///
		template<typename TFunction, typename TDiscreteSampler, typename TUniformSampler>
		bool Integrate(TOutput& result, const TFunction& f, const TDiscreteSampler& discreteSampler,
					   const TUniformSampler& uniformSampler, const TInteger numIterations)
		{
			result = 0;

			if (numIterations <= 0 || f == nullptr)
			{
				return false;
			}

			if (samples.empty() || weights.empty())
			{
				return false;
			}

			// Now we have importance resamples
			for (TInteger i = 0; i < numIterations; ++i)
			{
				const auto chosenIndex = discreteSampler();
				const TReal q_x = normalizedWeights[chosenIndex];
				if (q_x <= 0)
				{
					// it shouldn't reach here.
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

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class ImportanceResamplingTest : public TestCollection
	{
	public:
		ImportanceResamplingTest() : TestCollection("Importance Resampling Test") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
