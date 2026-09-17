// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include "Types.h"

namespace hbe::time
{
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

/// @brief The instant the engine treats as its own beginning.
/// @details One epoch for the whole engine: task deadlines, Render's deltaTime and the log timestamp
///          all measure against this same value, so a deadline set by one subsystem cannot disagree
///          with another reading the same span. Deliberately steady_clock rather than the wall clock,
///          because an epoch that jumps when the system clock is adjusted would silently expire - or
///          never expire - every deadline in flight.
/// @note Reachable without Engine::Get, which asserts that an instance exists. Logging has to keep
///       working in a process that never constructed an Engine, and a clock requiring one would
///       reintroduce exactly that crash. If nothing has set it, the epoch is the first instant it was
///       asked for, so readings stay meaningful instead of counting from 1970.
/// @threadsafe Readable from any thread, which is the point of it. ResetEngineEpoch is for the thread
///             constructing the engine.
[[nodiscard]] TEngineTimePoint GetEngineEpoch() noexcept;

/// @brief Move the epoch to the current instant.
/// @note Engine's constructor calls this so the epoch is engine construction rather than process load.
///       Call it once, before anything has measured against it: moving it afterwards shifts every
///       earlier reading relative to the later ones, which shows up as log lines that seem to run
///       backwards.
void ResetEngineEpoch() noexcept;

/// @brief Time elapsed since the epoch, for frame deltas and deadline checks.
[[nodiscard]] TEngineDuration ElapsedSinceEngineEpoch() noexcept;

/// @brief Target frame rate of the base stream, in hertz.
/// @details The engine-wide yardstick for CPU budgets: a stream compares the CPU time it has accumulated
///          against this period to decide whether to take more work, so one number governs every stream
///          and they cannot drift apart. Read it with GetBaseFramePeriod - the rate is what a config
///          holds, the period is what a deadline wants.
/// @note Until something sets it, the engine's default target rate applies. A stream must never read this
///       as "the frame rate I am actually achieving"; it is the target, and nothing here measures frames.
[[nodiscard]] double GetBaseFrameRate() noexcept;

/// @brief Target frame period of the base stream, derived from GetBaseFrameRate.
/// @details This is the number budgets are measured against, which is why it is exposed as a duration
///          rather than as a rate: comparing an accumulated CPU span to a rate invites an inverted
///          comparison that still compiles and silently inverts every budget in the engine.
[[nodiscard]] std::chrono::duration<double> GetBaseFramePeriod() noexcept;

/// @brief Set the base stream's target frame rate, which moves the budget yardstick with it.
/// @note The base stream's owner calls this during its configuration; everything else reads. Callers that
///       are not that owner setting it per frame would make every budget in the engine chase a moving
///       target, which reads as work randomly being accepted and refused.
/// @note A rate that is zero or negative is ignored, because a period of infinity silently disables every
///       budget while looking like a normal configuration value.
void SetBaseFrameRate(double hertz) noexcept;

} // namespace hbe::time

#ifdef __UNIT_TEST__
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
#endif //__UNIT_TEST__
