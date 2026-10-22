// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "PerfHarness.h"

#ifdef TEST_ENABLED
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <thread>

#include "Config/BuildConfig.h"
#include "Core/CommonMacros.h"

namespace hbe::Test
{
namespace
{
constexpr std::size_t FieldWidth = 11;
constexpr std::size_t NameWidth = 28;
constexpr std::size_t LineCapacity = 256;

[[nodiscard]] std::size_t Tenths(const PerfSample& inSample) noexcept
{
	return inSample.items == 0 ? 0 : (inSample.nanoseconds * 10 + inSample.items / 2) / inSample.items;
}

void WriteNsField(char* const ioField, const std::size_t inTenths) noexcept
{
	std::snprintf(ioField, FieldWidth, "%zu.%zu", inTenths / 10, inTenths % 10);
}

void WriteRatioField(char* const ioField, const std::size_t inRatioPercent)
{
	char text[LineCapacity];

	std::snprintf(text, sizeof(text), "%zu.%02zux", inRatioPercent / 100, inRatioPercent % 100);
	std::strncpy(ioField, text, FieldWidth - 1);
	ioField[FieldWidth - 1] = '\0';
}

[[nodiscard]] const char* GradeName(const EPerfGrade inGrade) noexcept
{
	switch (inGrade)
	{
		case EPerfGrade::Green:
			return "GREEN";
		case EPerfGrade::Amber:
			return "AMBER";
		case EPerfGrade::Red:
			return "RED";
		case EPerfGrade::Noise:
			return "NOISE";
		case EPerfGrade::Rejected:
			return "REJECTED";
		case EPerfGrade::None:
		default:
			return "report only";
	}
}

[[nodiscard]] const char* BuildName() noexcept
{
#if RELEASE_BUILD
	return "Release";
#elif DEBUG_BUILD
	return "Debug";
#else
	return "Dev";
#endif
}
} // namespace

PerfBenchmark::PerfBenchmark(const std::size_t inRounds) noexcept
	: arms{}
	, armCount(0)
	, rounds(inRounds > MaxRounds ? MaxRounds : inRounds)
	, runNanoseconds(0)
	, expectedWinner(nullptr)
	, expectedLoser(nullptr)
	, controlArm(nullptr)
	, expectedMinRatioPercent(100)
{
}

void PerfBenchmark::AddArm(const char* const inName, const void* const inUserData,
						   const TPerfMeasure inMeasure) noexcept
{
	if (armCount >= MaxArms)
	{
		return;
	}

	PerfArm& arm = arms[armCount++];

	arm.name = inName;
	arm.userData = inUserData;
	arm.measure = inMeasure;
	arm.violations = 0;

	for (std::size_t round = 0; round < MaxRounds; ++round)
	{
		arm.tenths[round] = 0;
	}
}

void PerfBenchmark::Expect(const char* const inWinner, const char* const inLoser,
						   const std::size_t inMinRatioPercent) noexcept
{
	expectedWinner = inWinner;
	expectedLoser = inLoser;
	expectedMinRatioPercent = inMinRatioPercent;
}

void PerfBenchmark::Run() noexcept
{
	const auto began = std::chrono::steady_clock::now();

	for (std::size_t round = 0; round < rounds; ++round)
	{
		for (std::size_t step = 0; step < armCount; ++step)
		{
			const std::size_t index = (step + round) % armCount;
			const PerfSample sample = arms[index].measure(arms[index].userData);

			arms[index].tenths[round] = Tenths(sample);
			arms[index].violations += sample.violations;
		}
	}

	runNanoseconds = static_cast<std::size_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began).count());
}

void PerfBenchmark::Control(const char* const inArmName) noexcept
{
	controlArm = inArmName;
}

std::size_t PerfBenchmark::IndexOf(const char* const inName) const noexcept
{
	returnValueIf(MaxArms, inName == nullptr);

	for (std::size_t index = 0; index < armCount; ++index)
	{
		continueIf(arms[index].name != inName && std::strcmp(arms[index].name, inName) != 0);

		return index;
	}

	return MaxArms;
}

std::size_t PerfBenchmark::BestOf(const PerfArm& inArm) const noexcept
{
	std::size_t best = std::numeric_limits<std::size_t>::max();

	for (std::size_t round = 0; round < rounds; ++round)
	{
		continueIf(inArm.tenths[round] >= best);

		best = inArm.tenths[round];
	}

	return best == std::numeric_limits<std::size_t>::max() ? 0 : best;
}

std::size_t PerfBenchmark::WorstOf(const PerfArm& inArm) const noexcept
{
	std::size_t worst = 0;

	for (std::size_t round = 0; round < rounds; ++round)
	{
		continueIf(inArm.tenths[round] <= worst);

		worst = inArm.tenths[round];
	}

	return worst;
}

std::size_t PerfBenchmark::MedianOf(const PerfArm& inArm) const noexcept
{
	std::array<std::size_t, MaxRounds> sorted{};

	for (std::size_t round = 0; round < rounds; ++round)
	{
		sorted[round] = inArm.tenths[round];
	}

	std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(rounds));

	return sorted[rounds / 2];
}

std::size_t PerfBenchmark::SpreadPercent(const PerfArm& inArm) const noexcept
{
	const std::size_t best = BestOf(inArm);

	return best == 0 ? 0 : (WorstOf(inArm) - best) * 100 / best;
}

std::size_t PerfBenchmark::ControlSpreadPercent() const noexcept
{
	const std::size_t control = IndexOf(controlArm);

	if (control < MaxArms)
	{
		return SpreadPercent(arms[control]);
	}

	const std::size_t winner = IndexOf(expectedWinner);
	std::size_t spread = 0;

	for (std::size_t index = 0; index < armCount; ++index)
	{
		continueIf(index == winner);

		const std::size_t armSpread = SpreadPercent(arms[index]);

		continueIf(armSpread <= spread);

		spread = armSpread;
	}

	return spread;
}

EPerfGrade PerfBenchmark::GradeOf(const PerfArm& inWinner, const PerfArm& inLoser) const noexcept
{
	if (inWinner.violations != 0 || BestOf(inWinner) == 0)
	{
		return EPerfGrade::Rejected;
	}

	const std::size_t bestRatio = BestOf(inLoser) * 100 / BestOf(inWinner);
	const std::size_t medianRatio = MedianOf(inLoser) * 100 / MedianOf(inWinner);

	if (bestRatio < expectedMinRatioPercent)
	{
		return EPerfGrade::Red;
	}

	const std::size_t control = ControlSpreadPercent();
	const std::size_t wobble = SpreadPercent(inWinner);
	const std::size_t uncertainty = controlArm != nullptr || wobble <= control ? control : wobble;
	const std::size_t capped = uncertainty > 100 ? 100 : uncertainty;

	if (bestRatio * (100 - capped) / 100 < expectedMinRatioPercent)
	{
		return EPerfGrade::Noise;
	}

	return medianRatio >= expectedMinRatioPercent ? EPerfGrade::Green : EPerfGrade::Amber;
}

EPerfGrade PerfBenchmark::GetGrade() const noexcept
{
	const std::size_t winner = IndexOf(expectedWinner);
	const std::size_t loser = IndexOf(expectedLoser);

	if (winner >= MaxArms || loser >= MaxArms)
	{
		return EPerfGrade::None;
	}

	return GradeOf(arms[winner], arms[loser]);
}

void PerfBenchmark::Report(std::ostream& ioOut) const
{
	char line[LineCapacity];
	char bestField[FieldWidth];
	char medianField[FieldWidth];
	char worstField[FieldWidth];
	char ratioField[FieldWidth];

	std::snprintf(line, sizeof(line), "  %-*s %*s %*s %*s %*s\n", static_cast<int>(NameWidth), "arm",
				  static_cast<int>(FieldWidth), "best", static_cast<int>(FieldWidth), "median",
				  static_cast<int>(FieldWidth), "slowest", static_cast<int>(FieldWidth), "viol");
	ioOut << line;

	for (std::size_t index = 0; index < armCount; ++index)
	{
		const PerfArm& arm = arms[index];

		WriteNsField(bestField, BestOf(arm));
		WriteNsField(medianField, MedianOf(arm));
		WriteNsField(worstField, WorstOf(arm));

		std::snprintf(line, sizeof(line), "  %-*s %*s %*s %*s %*zu\n", static_cast<int>(NameWidth), arm.name,
					  static_cast<int>(FieldWidth), bestField, static_cast<int>(FieldWidth), medianField,
					  static_cast<int>(FieldWidth), worstField, static_cast<int>(FieldWidth), arm.violations);
		ioOut << line;
	}

	char wall[FieldWidth];

	std::snprintf(wall, sizeof(wall), "%.2f", static_cast<double>(runNanoseconds) / 1000000000.0);
	std::snprintf(line, sizeof(line), "  cell %s s over %zu rounds and %zu arms\n", wall, rounds, armCount);
	ioOut << line;

	if (GetGrade() == EPerfGrade::None)
	{
		return;
	}

	const std::size_t winner = IndexOf(expectedWinner);
	const std::size_t loser = IndexOf(expectedLoser);
	const std::size_t bestRatio = BestOf(arms[loser]) * 100 / BestOf(arms[winner]);
	const std::size_t medianRatio = MedianOf(arms[loser]) * 100 / MedianOf(arms[winner]);
	const char* const noiseLabel = controlArm != nullptr ? "control" : "spread ";

	WriteRatioField(ratioField, expectedMinRatioPercent);

	std::snprintf(line, sizeof(line),
				  "  expect %s >= %s %s | best %zu.%02zux  median %zu.%02zux  wobble %zu%%  %s %zu%%  -> %s\n",
				  expectedLoser, ratioField, expectedWinner, bestRatio / 100, bestRatio % 100, medianRatio / 100,
				  medianRatio % 100, SpreadPercent(arms[winner]), noiseLabel, ControlSpreadPercent(),
				  GradeName(GetGrade()));
	ioOut << line;
}

void PerfPlatformHeader(std::ostream& ioOut)
{
	char line[LineCapacity];

	std::snprintf(line, sizeof(line),
				  "platform %d logical CPUs | line %d bytes | build %s | arms rotate each round,"
				  " ns/item is best of rounds, control is a same-code duplicate arm\n",
				  static_cast<int>(std::thread::hardware_concurrency()),
				  static_cast<int>(std::hardware_destructive_interference_size), BuildName());
	ioOut << line;
}
} // namespace hbe::Test

#endif // TEST_ENABLED
