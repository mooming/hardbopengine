// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "MonteCarloIntegrator.h"

#ifdef TEST_ENABLED
#include <random>

#include "Core/Constants.h"

void hbe::MonteCarloIntegrationTest::Prepare() noexcept
{
	AddTest("Calculate Pi", [this](auto& ls) -> void
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

		auto pdf = [](const Vec2& x) -> double
		{
			constexpr double p = 1.0 / 4.0f;

			return p;
		};

		auto randomGen = []() -> Vec2
		{
			static std::random_device rd;
			static std::mt19937 gen(rd());
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
		}

		average /= numRepeat;
		double variance = 0;
		for (auto item : results)
		{
			auto delta = item - average;
			variance += (delta * delta);
		}

		variance /= numRepeat;

		ls << "MC Samples = " << numIterations << ", Average = " << average << ", Std. Deviation = " << sqrt(variance)
		   << lf;
	});

	AddTest("Calculate Pi (2)", [this](auto& ls) -> void
	{
		auto func = [](const double& x) -> double
		{
			if (x < 0 || x > 1)
			{
				return 0;
			}

			return std::sqrt(1 - (x * x)) * 4.0f;
		};

		auto pdf = [](const double&) -> double { return 0.5; };

		auto randomGen = []() -> double
		{
			static std::random_device rd;
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
		}

		average /= numRepeat;
		double variance = 0;
		for (auto item : results)
		{
			auto delta = item - average;
			variance += (delta * delta);
		}

		variance /= numRepeat;

		ls << "MC Samples = " << numIterations << ", Average = " << average << ", Std. Deviation = " << sqrt(variance)
		   << lf;
	});
}
#endif // TEST_ENABLED
