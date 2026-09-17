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
