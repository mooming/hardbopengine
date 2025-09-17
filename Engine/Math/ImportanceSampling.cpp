//
// Created by mooming on 22/06/2025.
//

#include "ImportanceResampling.h"
#include "StratifiedSampling.h"

#ifdef __UNIT_TEST__
#include <random>
#include "Core/Constants.h"

void hbe::ImportanceResamplingTest::Prepare()
{
	constexpr uint32_t numSrcSamples = 200;
	constexpr uint32_t numResamplingIterations = 50;
	constexpr uint32_t numSubGroups = 10;
	constexpr uint32_t numRepeat = 5;

	auto func = [](const double& x) -> double
	{
		if (x < 0 || x > 1)
		{
			return 0;
		}

		return std::sqrt(1 - (x * x)) * 4.0f;
	};

	auto pdf = [](const double&) -> double
	{
		// uniform pdf in [0, 1]
		return 1.0;
	};

	static std::random_device rd;
	static std::mt19937 gen(rd());

	auto calculatePi = [&](auto& ls) -> void
	{
		auto randomGen = [&]() -> double
		{
			static std::uniform_real_distribution uniformDist(0.0, 1.0);
			return uniformDist(gen);
		};

		auto norm = [](const double& value) { return std::abs(value); };
		using IR = ImportanceResampling<double, double, double, uint32_t>;

		IR integrator;

		if (!integrator.Resample(func, pdf, randomGen, norm, numSrcSamples))
		{
			ls << "failed to perform MC integration importance resampling" << lferr;
			return;
		}

		auto& weights = integrator.GetWeights();
		auto& samples = integrator.GetSamples();

		if (weights.size() != samples.size())
		{
			ls << "number of weights and number of samples are not matched" << lferr;
			return;
		}

		double average = 0;
		double results[numRepeat];
		const uint32_t numSamples = samples.size();

		auto& normalizedWeights = integrator.GetNormalizedWeights();
		const double halfStep = 0.5 / numSamples;

		std::discrete_distribution<uint32_t> discreteDist(normalizedWeights.begin(), normalizedWeights.end());
		std::uniform_real_distribution<> unifromDist(-halfStep, halfStep);
		auto discreteSampler = [&discreteDist]() { return discreteDist(gen); };
		auto uniformSampler = [&unifromDist]() { return unifromDist(gen); };

		for (double& result : results)
		{
			if (!integrator.Integrate(result, func, discreteSampler, uniformSampler, numResamplingIterations))
			{
				ls << "failed to perform MC integration importance resampling" << lferr;
			}

			average += result;

			ls << "Samples = " << numSrcSamples << ", Resampling = " << numResamplingIterations
			   << ", result = " << result << " <=> PI " << Pi << ", Error = " << (abs(result - Pi) * 100 / Pi) << lf;
		}

		average /= numRepeat;
		double variance = 0;
		for (auto item : results)
		{
			auto delta = item - average;
			variance += (delta * delta);
		}

		variance /= numRepeat;

		ls << "Average = " << average << ", Std. Deviation = " << sqrt(variance)
		   << ", Error = " << (abs(average - Pi) * 100 / Pi) << lf;
	};

	AddTest("Calculate Pi", calculatePi);

	auto calculatePi_Stratified = [&](auto& ls) -> void
	{
		auto randomGenInRange = [&](double rangeStart, double rangeEnd) -> double
		{
			std::uniform_real_distribution rangeDist(rangeStart, rangeEnd);
			return rangeDist(gen);
		};

		StratifiedSampling<> stratifiedSampler(numSubGroups, 0, 1);

		auto randomGen = [&]() -> double
		{
			auto x = stratifiedSampler.Sample(randomGenInRange);
			stratifiedSampler.ChangeSubGroup();
			return x;
		};

		auto norm = [](const double& value) { return std::abs(value); };
		using IR = ImportanceResampling<double, double, double, uint32_t>;
		IR integrator;

		if (!integrator.Resample(func, pdf, randomGen, norm, numSrcSamples))
		{
			ls << "failed to perform MC integration importance resampling" << lferr;
			return;
		}

		auto& weights = integrator.GetWeights();
		auto& samples = integrator.GetSamples();

		if (weights.size() != samples.size())
		{
			ls << "number of weights and number of samples are not matched" << lferr;
			return;
		}

		double average = 0;
		double results[numRepeat];
		const uint32_t numSamples = samples.size();

		auto& normalizedWeights = integrator.GetNormalizedWeights();
		const double halfStep = 0.5 / numSamples;

		std::discrete_distribution<uint32_t> discreteDist(normalizedWeights.begin(), normalizedWeights.end());
		std::uniform_real_distribution<> unifromDist(-halfStep, halfStep);
		auto discreteSampler = [&discreteDist]() { return discreteDist(gen); };
		auto uniformSampler = [&unifromDist]() { return unifromDist(gen); };

		for (double& result : results)
		{
			if (!integrator.Integrate(result, func, discreteSampler, uniformSampler, numResamplingIterations))
			{
				ls << "failed to perform MC integration importance resampling" << lferr;
			}

			average += result;

			ls << "Samples = " << numSrcSamples << ", Resampling = " << numResamplingIterations
			   << ", result = " << result << " <=> PI " << Pi << ", Error = " << (abs(result - Pi) * 100 / Pi) << lf;
		}

		average /= numRepeat;
		double variance = 0;
		for (auto item : results)
		{
			auto delta = item - average;
			variance += (delta * delta);
		}

		variance /= numRepeat;

		ls << "Average = " << average << ", Std. Deviation = " << sqrt(variance)
		   << ", Error = " << (abs(average - Pi) * 100 / Pi) << lf;
	};

	AddTest("Calculate Pi(Stratified)", calculatePi_Stratified);

	auto sampleGrowthTest = [&](auto& ls) -> void
	{
		auto randomGenInRange = [&](double rangeStart, double rangeEnd) -> double
		{
			std::uniform_real_distribution rangeDist(rangeStart, rangeEnd);
			return rangeDist(gen);
		};

		StratifiedSampling<> stratifiedSampler(numSubGroups, 0, 1);

		auto randomGen = [&]() -> double
		{
			auto x = stratifiedSampler.Sample(randomGenInRange);
			stratifiedSampler.ChangeSubGroup();
			return x;
		};

		auto norm = [](const double& value) { return std::abs(value); };
		using IR = ImportanceResampling<double, double, double, uint32_t>;

		IR integrator;

		if (!integrator.Resample(func, pdf, randomGen, norm, numSrcSamples))
		{
			ls << "failed to perform MC integration importance resampling" << lferr;
			return;
		}

		auto& weights = integrator.GetWeights();
		auto& samples = integrator.GetSamples();
		uint32_t numSamples = samples.size();

		if (weights.size() != samples.size())
		{
			ls << "number of weights and number of samples are not matched" << lferr;
			return;
		}

		if (weights.empty())
		{
			ls << "empty resampling" << lferr;
			return;
		}

		auto& normalizedWeights = integrator.GetNormalizedWeights();
		const double halfStep = 0.5 / numSamples;

		std::discrete_distribution<uint32_t> discreteDist(normalizedWeights.begin(), normalizedWeights.end());
		std::uniform_real_distribution<> unifromDist(-halfStep, halfStep);
		auto discreteSampler = [&discreteDist]() { return discreteDist(gen); };
		auto uniformSampler = [&unifromDist]() { return unifromDist(gen); };

		constexpr int numGrowth = 10;

		for (int i = 0; i < numGrowth; ++i)
		{
			double average = 0;
			double results[numRepeat];

			for (auto& result : results)
			{
				if (!integrator.Integrate(result, func, discreteSampler, uniformSampler, numResamplingIterations))
				{
					ls << "failed to perform MC integration importance resampling" << lferr;
				}

				average += result;

				ls << "Samples = " << numSamples << ", Resampling = " << numResamplingIterations
				   << ", result = " << result << " <=> PI " << Pi << ", Error = " << (abs(result - Pi) * 100 / Pi)
				   << lf;
			}

			if (!integrator.Resample(func, pdf, randomGen, norm, numSrcSamples))
			{
				ls << "failed to perform MC integration importance resampling" << lferr;
				return;
			}

			numSamples = samples.size();

			average /= numRepeat;
			double variance = 0;
			for (auto item : results)
			{
				auto delta = item - average;
				variance += (delta * delta);
			}

			variance /= numRepeat;

			ls << "Average = " << average << ", Std. Deviation = " << sqrt(variance)
			   << ", Error = " << (abs(average - Pi) * 100 / Pi) << lf;
		}
	};

	AddTest("Calculate Pi (Growth)", sampleGrowthTest);

	auto sampleRebuildGrowthTest = [&](auto& ls) -> void
	{
		auto randomGenInRange = [&](double rangeStart, double rangeEnd) -> double
		{
			std::uniform_real_distribution rangeDist(rangeStart, rangeEnd);
			return rangeDist(gen);
		};

		StratifiedSampling<> stratifiedSampler(numSubGroups, 0, 1);

		auto randomGen = [&]() -> double
		{
			auto x = stratifiedSampler.Sample(randomGenInRange);
			stratifiedSampler.ChangeSubGroup();
			return x;
		};

		auto norm = [](const double& value) { return std::abs(value); };
		using IR = ImportanceResampling<double, double, double, uint32_t>;

		IR integrator;

		if (!integrator.Resample(func, pdf, randomGen, norm, numSrcSamples))
		{
			ls << "failed to perform MC integration importance resampling" << lferr;
			return;
		}

		auto& weights = integrator.GetWeights();
		auto& samples = integrator.GetSamples();
		uint32_t numSamples = samples.size();

		if (weights.size() != samples.size())
		{
			ls << "number of weights and number of samples are not matched" << lferr;
			return;
		}

		if (weights.empty())
		{
			ls << "empty resampling" << lferr;
			return;
		}

		auto& normalizedWeights = integrator.GetNormalizedWeights();
		const double halfStep = 0.5 / numSamples;

		std::discrete_distribution<uint32_t> discreteDist(normalizedWeights.begin(), normalizedWeights.end());
		std::uniform_real_distribution<> unifromDist(-halfStep, halfStep);
		auto discreteSampler = [&discreteDist]() { return discreteDist(gen); };
		auto uniformSampler = [&unifromDist]() { return unifromDist(gen); };

		constexpr int numGrowth = 10;

		for (int i = 0; i < numGrowth; ++i)
		{
			double average = 0;
			double results[numRepeat];

			for (auto& result : results)
			{
				if (!integrator.Integrate(result, func, discreteSampler, uniformSampler, numResamplingIterations))
				{
					ls << "failed to perform MC integration importance resampling" << lferr;
				}

				average += result;

				ls << "Samples = " << numSamples << ", Resampling = " << numResamplingIterations
				   << ", result = " << result << " <=> PI " << Pi << ", Error = " << (abs(result - Pi) * 100 / Pi)
				   << lf;
			}

			integrator.ClearResampledData();

			if (!integrator.Resample(func, pdf, randomGen, norm, numSrcSamples * (i + 1)))
			{
				ls << "failed to perform MC integration importance resampling" << lferr;
				return;
			}

			numSamples = samples.size();

			average /= numRepeat;
			double variance = 0;
			for (auto item : results)
			{
				auto delta = item - average;
				variance += (delta * delta);
			}

			variance /= numRepeat;

			ls << "Average = " << average << ", Std. Deviation = " << sqrt(variance)
			   << ", Error = " << (abs(average - Pi) * 100 / Pi) << lf;
		}
	};

	AddTest("Calculate Pi (Rebuild Growth)", sampleRebuildGrowthTest);
}
#endif // __UNIT_TEST__
