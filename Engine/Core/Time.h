// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>

#include "Types.h"


namespace hbe::time
{
/// API reference: docs/Core/index.html#time
using TMilliSec = uint64_t;
using TStopWatch = std::chrono::high_resolution_clock;
using TTime = std::chrono::time_point<TStopWatch>;
using TDuration = TStopWatch::duration;
using TMilliSecs = std::chrono::milliseconds;

[[nodiscard]] inline TTime GetNow() noexcept
{
	return std::chrono::steady_clock::now();
}

[[nodiscard]] inline float ToFloat(TStopWatch::duration duration) noexcept
{
	std::chrono::duration<float> delta = duration;

	return delta.count();
}

[[nodiscard]] inline double ToDouble(TStopWatch::duration duration) noexcept
{
	std::chrono::duration<double> delta = duration;

	return delta.count();
}

[[nodiscard]] inline TMilliSecs::rep ToMilliSeconds(TStopWatch::duration duration) noexcept
{
	auto delta = std::chrono::duration_cast<TMilliSecs>(duration);

	return delta.count();
}

void Sleep(TMilliSec milli) noexcept;

using TEngineTimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using TEngineDuration = TEngineTimePoint::duration;

[[nodiscard]] TEngineTimePoint GetEngineEpoch() noexcept;

void ResetEngineEpoch() noexcept;

[[nodiscard]] TEngineDuration ElapsedSinceEngineEpoch() noexcept;

[[nodiscard]] double GetBaseFrameRate() noexcept;

[[nodiscard]] std::chrono::duration<double> GetBaseFramePeriod() noexcept;

void SetBaseFrameRate(double hertz) noexcept;
} // namespace hbe::time

#ifdef TEST_ENABLED
#include "Test/TestCollection.h"

namespace hbe
{
class TimeTest : public TestCollection
{
public:
	TimeTest()
		: TestCollection("TimeTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif //TEST_ENABLED
