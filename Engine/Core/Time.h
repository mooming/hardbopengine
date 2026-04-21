// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <chrono>
#include "Types.h"


namespace hbe::time
{
	using MilliSec = uint64_t;
	using TStopWatch = std::chrono::high_resolution_clock;
	using TTime = std::chrono::time_point<TStopWatch>;
	using TDuration = TStopWatch::duration;
	using TMilliSecs = std::chrono::milliseconds;

	TTime GetNow() { return std::chrono::steady_clock::now(); }

	float ToFloat(TStopWatch::duration duration)
	{
		std::chrono::duration<float> delta = duration;
		return delta.count();
	}

	double ToDouble(TStopWatch::duration duration)
	{
		std::chrono::duration<double> delta = duration;
		return delta.count();
	}

	TMilliSecs::rep ToMilliSeconds(TStopWatch::duration duration)
	{
		auto delta = std::chrono::duration_cast<TMilliSecs>(duration);
		return delta.count();
	}

	void Sleep(MilliSec milli);
} // namespace Time

