//
// Created by mooming on 21/06/2025.
//

#include "MonteCarloIntegrator.h"

#ifdef __UNIT_TEST__
#include <random>
#include "System/Constants.h"

void HE::MonteCarloIntegrationTest::Prepare()
{
	AddTest("Calculate Pi",
			[this](auto& ls) -> void
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

				auto randomGen = []() -> Vec2
				{
					static std::random_device rd; // random seed
					static std::mt19937 gen(rd()); // pseudo random gen
					static std::uniform_real_distribution dist(-1.0, 1.0);

					Vec2 x;
					x.x = dist(gen);
					x.y = dist(gen);
					return x;
				};

				double result = 0;
				using MCI = MonteCarloIntegrator<Vec2, double, double, uint32_t>;
				constexpr uint32_t numIterations = 1024;

				MCI Integrator;

				constexpr int numRepeat = 10;
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

				ls << "Average = " << average << ", Std. Deviation = " << sqrt(variance) << lf;
			});

	AddTest("Calculate Pi (2)",
			[this](auto& ls) -> void
			{
				auto func = [](const double& x) -> double
				{
					if (x < 0 || x > 1)
					{
						return 0;
					}

					return std::sqrt(1 - (x * x)) * 4.0f;
				};

				// uniform pdf in [0, 2]
				auto pdf = [](const double&) -> double { return 0.5; };

				auto randomGen = []() -> double
				{
					// pseudo random gen
					static std::random_device rd; // random seed
					static std::mt19937 gen(rd());
					static std::uniform_real_distribution dist(0.0, 2.0);

					return dist(gen);
				};

				double result = 0;
				using MCI = MonteCarloIntegrator<double, double, double, uint32_t>;
				constexpr uint32_t numIterations = 1024;

				MCI Integrator;

				constexpr int numRepeat = 10;
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

				ls << "Average = " << average << ", Std. Deviation = " << sqrt(variance) << lf;
			});
}
#endif // __UNIT_TEST__
