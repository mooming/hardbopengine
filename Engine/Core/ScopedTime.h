// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Time.h"

namespace hbe::time
{
	/// @brief A RAII-style scoped timer that measures execution time within a scope.
	class ScopedTime final
	{
	private:
		TDuration& duration;
		TTime start;

	public:
		ScopedTime(const ScopedTime&) = delete;

		explicit ScopedTime(TDuration& outDeltaTime) : duration(outDeltaTime), start(TStopWatch::now()) {}

		~ScopedTime()
		{
			TTime end = TStopWatch::now();
			duration = end - start;
		}
	};
} // namespace hbe::Time

