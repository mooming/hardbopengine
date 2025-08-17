//
// Created by mooming on 21/06/2025.
//

#include "StratifiedSampling.h"

#ifdef __UNIT_TEST__

#include <random>

#include "Memory/MultiPoolAllocator.h"
#include "MonteCarloIntegrator.h"
#include "System/Constants.h"

void hbe::StratifiedSamplingTest::Prepare()
{
	auto calculatePi = [this](auto& ls) -> void
	{
		struct Vec2 final
		{
			double x = 0;
			double y = 0;
		};

		auto func = [](const Vec2& x) -> double
		{
			double r = x.x * x.x + x.y * x.y;
			return r <= 1.0 ? 1.0 : 0.0f;
		};

		// uniform pdf in (-1, -1) to (1, 1)
		auto pdf = [](const Vec2& x) -> double
		{
			constexpr double p = 1.0 / 4.0f;
			return p;
		};

		constexpr uint32_t numIterations = 512;
		constexpr uint32_t numSubGroups = 5;
		StratifiedSampling<> samplerForX(numSubGroups, -1, 1);
		StratifiedSampling<> samplerForY(numSubGroups, -1, 1);

		auto stratifiedSampler = [&samplerForX, &samplerForY]() -> Vec2
		{
			auto randomGenInRange = [](double rangeStart, double rangeEnd) -> double
			{
				static std::random_device rd; // random seed
				static std::mt19937 gen(rd()); // pseudo random gen
				std::uniform_real_distribution dist(rangeStart, rangeEnd);

				double value = dist(gen);
				return value;
			};

			Vec2 v(samplerForX.Sample(randomGenInRange), samplerForY.Sample(randomGenInRange));
			samplerForX.ChangeSubGroup();

			if (samplerForX.GetSubGroupIndex() == 0)
			{
				samplerForY.ChangeSubGroup();
			}

			return v;
		};

		double result = 0;
		using MCI = MonteCarloIntegrator<Vec2, double, double, uint32_t>;
		MCI Integrator;

		constexpr int numRepeat = 10;
		{
			auto randomGen = []()
			{
				static std::random_device rd; // random seed
				static std::mt19937 gen(rd()); // pseudo random gen
				static std::uniform_real_distribution dist(-1.0, 1.0);

				Vec2 value(dist(gen), dist(gen));
				return value;
			};

			double average = 0;
			double results[numRepeat];

			for (double& i : results)
			{
				bool bOk = Integrator(result, func, pdf, randomGen, numIterations);
				if (!bOk)
				{
					ls << "failed to perform MC integration" << lferr;
					return;
				}

				i = result;
				average += result;

				ls << "MC Samples = " << numIterations << ", result = " << result << ", PI " << Pi << lf;
			}

			average /= numRepeat;
			double variance = 0;
			for (auto item : results)
			{
				auto delta = item - average;
				variance += (delta * delta);
			}

			variance /= numRepeat;

			ls << "Uniform Random: Average = " << average << ", Std. Deviation = " << sqrt(variance) << lf;
		}

		{

			double average = 0;
			double results[numRepeat];

			for (double& i : results)
			{
				bool bOk = Integrator(result, func, pdf, stratifiedSampler, numIterations);
				if (!bOk)
				{
					ls << "failed to perform MC integration" << lferr;
					return;
				}

				i = result;
				average += result;

				ls << "MC Samples = " << numIterations << ", result = " << result << ", PI " << Pi << lf;
			}

			average /= numRepeat;
			double variance = 0;
			for (auto item : results)
			{
				auto delta = item - average;
				variance += (delta * delta);
			}

			variance /= numRepeat;

			ls << "Stratified Random: Average = " << average << ", Std. Deviation = " << sqrt(variance) << lf;
		}
	};

	AddTest("Calculate Pi Comparison", calculatePi);
}
#endif // __UNIT_TEST__
