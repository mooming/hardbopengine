// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Time.h"

#include <atomic>
#include <chrono>
#include <thread>


namespace hbe
{
namespace
{
std::atomic<long long>& EpochNanosStorage() noexcept
{
	const auto sinceEpoch = std::chrono::steady_clock::now().time_since_epoch();
	const auto nanos = static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(sinceEpoch).count());

	static std::atomic<long long> epochNanos{nanos};

	return epochNanos;
}

constexpr double defaultBaseFrameRate = 60.0;

std::atomic<double>& BaseFrameRateStorage() noexcept
{
	static std::atomic<double> rate{defaultBaseFrameRate};

	return rate;
}
} // namespace

void time::Sleep(time::TMilliSec milli) noexcept
{
	using namespace std::chrono;
	std::this_thread::sleep_for(std::chrono::milliseconds(milli));
}

time::TEngineTimePoint time::GetEngineEpoch() noexcept
{
	return time::TEngineTimePoint{std::chrono::nanoseconds{EpochNanosStorage().load(std::memory_order_acquire)}};
}

void time::ResetEngineEpoch() noexcept
{
	const auto sinceEpoch = std::chrono::steady_clock::now().time_since_epoch();
	const auto nanos = static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(sinceEpoch).count());
	EpochNanosStorage().store(nanos, std::memory_order_release);
}

time::TEngineDuration time::ElapsedSinceEngineEpoch() noexcept
{
	return std::chrono::steady_clock::now() - GetEngineEpoch();
}

double time::GetBaseFrameRate() noexcept
{
	return BaseFrameRateStorage().load(std::memory_order_acquire);
}

std::chrono::duration<double> time::GetBaseFramePeriod() noexcept
{
	const auto hertz = GetBaseFrameRate();
	if (hertz <= 0.0)
	{
		return std::chrono::duration<double>{};
	}

	return std::chrono::duration<double>{1.0 / hertz};
}

void time::SetBaseFrameRate(double hertz) noexcept
{
	if (hertz <= 0.0)
	{
		return;
	}

	BaseFrameRateStorage().store(hertz, std::memory_order_release);
}
} // namespace hbe

#ifdef __UNIT_TEST__
namespace hbe
{
void TimeTest::Prepare()
{
	AddTest("Elapsed time advances", [this](auto& ls)
	{
		const auto first = time::ElapsedSinceEngineEpoch();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
		const auto second = time::ElapsedSinceEngineEpoch();

		const auto advanced = second - first;
		if (advanced < std::chrono::milliseconds(1))
		{
			ls << "Elapsed advanced by only " << std::chrono::duration_cast<std::chrono::microseconds>(advanced).count()
			   << " us across a 5 ms sleep." << lferr;
		}
	});

	AddTest("Epoch is engine start, not the clock epoch", [this](auto& ls)
	{
		const auto elapsed = time::ElapsedSinceEngineEpoch();
		if (elapsed > std::chrono::hours(1))
		{
			ls << "Elapsed since the engine epoch is "
			   << std::chrono::duration_cast<std::chrono::hours>(elapsed).count()
			   << " hours; the epoch is reading a zero value rather than engine start - this is the D5 defect."
			   << lferr;
		}
	});

	AddTest("ResetEngineEpoch moves the zero point", [this](auto& ls)
	{
		time::ResetEngineEpoch();
		std::this_thread::sleep_for(std::chrono::milliseconds(30));

		const auto beforeReset = time::ElapsedSinceEngineEpoch();
		time::ResetEngineEpoch();
		const auto afterReset = time::ElapsedSinceEngineEpoch();

		if (beforeReset < std::chrono::milliseconds(20))
		{
			ls << "Elapsed did not accumulate while the epoch stood still: "
			   << std::chrono::duration_cast<std::chrono::milliseconds>(beforeReset).count()
			   << " ms after a 30 ms sleep." << lferr;
		}

		if (afterReset >= std::chrono::milliseconds(20))
		{
			ls << "Elapsed was not reset: " << std::chrono::duration_cast<std::chrono::milliseconds>(afterReset).count()
			   << " ms measured immediately after resetting the epoch." << lferr;
		}
	});

	AddTest("Base frame period is the reciprocal of the rate", [this](auto& ls)
	{
		time::SetBaseFrameRate(120.0);

		const auto period = time::GetBaseFramePeriod();
		const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(period).count();
		ls << "At 120 Hz the budget yardstick is " << micros << " us." << lf;

		if (period < std::chrono::duration<double, std::micro>{8000} ||
			period > std::chrono::duration<double, std::micro>{8400})
		{
			ls << "120 Hz produced a period of " << micros << " us, not the 8333 us a budget is measured against."
			   << lferr;
		}

		time::SetBaseFrameRate(0.0);

		if (time::GetBaseFrameRate() != 120.0)
		{
			ls << "A zero frame rate was accepted, which makes the yardstick infinite and silently disables every "
				  "budget."
			   << lferr;
		}

		time::SetBaseFrameRate(60.0);
	});
}
} // namespace hbe
#endif //__UNIT_TEST__
