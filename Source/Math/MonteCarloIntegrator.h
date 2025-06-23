//
// Created by mooming on 21/06/2025.
//

#pragma once

#include <cstdint>
#include <type_traits>

namespace HE
{

	template<typename TInput = double, typename TOutput = double, typename TReal = double, typename TInteger = uint32_t>
	class MonteCarloIntegrator final
	{
	public:
		static_assert(std::is_floating_point_v<TReal>);
		static_assert(std::is_integral_v<TInteger>);

	public:
		MonteCarloIntegrator() = default;
		~MonteCarloIntegrator() = default;

		// TFunction - TOutput f(const TInput& input), A general function
		// TPDF - TReal p(const TInput& input), Probability Density Function
		// TRandomGen - TInput sample(), return a random number
		template<typename TFunction, typename TPDF, typename TRandomGen>
		bool operator()(TOutput& result, const TFunction& f, const TPDF& p, const TRandomGen& sample,
						const TInteger numIterations)
		{
			result = 0;

			if (numIterations <= 0)
			{
				return false;
			}

			for (TInteger i = 0; i < numIterations; ++i)
			{
				const TInput x = sample();
				const TReal p_x = p(x);
				if (p_x <= 0)
				{
					return false;
				}

				const TOutput f_x = f(x);
				const TOutput fx_over_px = f_x / p_x;

				result += fx_over_px;
			}

			result /= numIterations;

			return true;
		}
	};

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

	class MonteCarloIntegrationTest : public TestCollection
	{
	public:
		MonteCarloIntegrationTest() : TestCollection("Monte Carlo Integration Test") {}

	protected:
		void Prepare() override;
	};
} // namespace HE
#endif //__UNIT_TEST__
