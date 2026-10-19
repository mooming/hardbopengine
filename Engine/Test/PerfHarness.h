// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifndef __HBE_TEST_PERF_HARNESS__
#define __HBE_TEST_PERF_HARNESS__

#ifdef TEST_ENABLED
#include <cstddef>
#include <ostream>

namespace hbe::Test
{
/// API reference: docs/Test/EPerfGrade/index.html
enum class EPerfGrade : unsigned char
{
	None,
	Green,
	Amber,
	Red,
	Noise,
	Rejected
};

/// API reference: docs/Test/PerfSample/index.html
struct PerfSample
{
	std::size_t nanoseconds;
	std::size_t items;
	std::size_t violations;
};

using TPerfMeasure = PerfSample (*)(const void* inUserData);

/// API reference: docs/Test/PerfBenchmark/index.html
class PerfBenchmark final
{
public:
	static constexpr std::size_t MaxArms = 5;
	static constexpr std::size_t MaxRounds = 11;

private:
	struct PerfArm // hb-standards:ignore
	{
		const char* name;
		const void* userData;
		TPerfMeasure measure;
		std::size_t tenths[MaxRounds];
		std::size_t violations;
	};

	PerfArm arms[MaxArms];
	std::size_t armCount;
	std::size_t rounds;
	std::size_t runNanoseconds;
	const char* expectedWinner;
	const char* expectedLoser;
	const char* controlArm;
	std::size_t expectedMinRatioPercent;

public:
	explicit PerfBenchmark(std::size_t inRounds) noexcept;

	void AddArm(const char* inName, const void* inUserData, TPerfMeasure inMeasure) noexcept;
	void Expect(const char* inWinner, const char* inLoser, std::size_t inMinRatioPercent) noexcept;
	void Control(const char* inArmName) noexcept;
	void Run() noexcept;
	void Report(std::ostream& ioOut) const;

	[[nodiscard]] EPerfGrade GetGrade() const noexcept;

private:
	[[nodiscard]] std::size_t IndexOf(const char* inName) const noexcept;
	[[nodiscard]] std::size_t BestOf(const PerfArm& inArm) const noexcept;
	[[nodiscard]] std::size_t WorstOf(const PerfArm& inArm) const noexcept;
	[[nodiscard]] std::size_t MedianOf(const PerfArm& inArm) const noexcept;
	[[nodiscard]] std::size_t SpreadPercent(const PerfArm& inArm) const noexcept;
	[[nodiscard]] std::size_t ControlSpreadPercent() const noexcept;
	[[nodiscard]] EPerfGrade GradeOf(const PerfArm& inWinner, const PerfArm& inLoser) const noexcept;
};

void PerfPlatformHeader(std::ostream& ioOut);
} // namespace hbe::Test

#endif // TEST_ENABLED

#endif // __HBE_TEST_PERF_HARNESS__
