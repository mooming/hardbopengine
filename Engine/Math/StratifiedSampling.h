//
// Created by mooming on 22/06/2025.
//

#pragma once

#include <random>
#include "HSTL/HVector.h"

namespace hbe
{

	template<typename TReal = double, typename TInteger = uint32_t>
	class StratifiedSampling
	{
	public:
		static_assert(std::is_floating_point_v<TReal>);
		static_assert(std::is_integral_v<TInteger>);

	private:
		TInteger numSubGroups;
		TInteger subGroupIndex;
		TReal interval;
		TReal start;
		TReal end;

	public:
		StratifiedSampling() : numSubGroups(0), subGroupIndex(0), interval(0), start(0), end(0) {}
		StratifiedSampling(TInteger inNumSubGroups, TReal start, TReal end) :
			numSubGroups(inNumSubGroups > 0 ? inNumSubGroups : 1), subGroupIndex(0),
			interval((end - start) / numSubGroups), start(start), end(end)
		{}

		~StratifiedSampling() = default;

		TInteger GetSubGroupIndex() const { return subGroupIndex; }

		// TRangeSampler - TReal sampler(TReal rangeStart, TReal rangeEnd), return a random number within the given
		// range
		template<typename TRangeSampler>
		TReal Sample(const TRangeSampler& sampler)
		{
			const TReal rangeStart = start + (interval * subGroupIndex);
			const TReal rangeEnd = rangeStart + interval;

			return sampler(rangeStart, rangeEnd);
		}

		void ChangeSubGroup(TInteger delta = 1)
		{
			subGroupIndex += delta;

			if (subGroupIndex <= 0 || subGroupIndex >= numSubGroups)
			{
				subGroupIndex = 0;
			}
		}
	};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
	class StratifiedSamplingTest : public TestCollection
	{
	public:
		StratifiedSamplingTest() : TestCollection("Stratified Sampling Test") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
