// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Config/BuildConfig.h"
#include "Config/EngineConfig.h"
#include "Log/Logger.h"
#include "String/StringUtil.h"
#include "Time.h"

namespace hbe
{

	template<typename T>
	concept CLockable = requires(T t) {
		t.lock();
		t.unlock();
	};

	template<CLockable TLockable>
	class ScopedLock final
	{
		using TSrcLoc = hbe::source_location;

	private:
		TLockable& lockable;

#if PROFILE_ENABLED
		time::TTime startTime;
		float timeOutSec;
		TSrcLoc srcLoc;
#endif // PROFILE_ENABLED

	public:
		ScopedLock(const ScopedLock&) = delete;

#if PROFILE_ENABLED
		explicit ScopedLock(TLockable& lockable, float timeOutSec = 0.01f, const TSrcLoc& srcLoc = TSrcLoc::current()) :
			lockable(lockable), startTime(time::TStopWatch::now()), timeOutSec(timeOutSec), srcLoc(srcLoc)
		{
			lockable.lock();
#ifdef __DEBUG__
			ScopedLock::timeOutSec *= Config::DebugTimeOutMultiplier;
#endif // __DEBUG__
		}
#else // PROFILE_ENABLED
		ScopedLock(TLockable& lockable) : lockable(lockable) { lockable.lock(); }

		ScopedLock(TLockable& lockable, float) : ScopedLock(lockable) {}

		ScopedLock(TLockable& lockable, float, const TSrcLoc&) : ScopedLock(lockable) {}
#endif // PROFILE_ENABLED

		~ScopedLock()
		{
			lockable.unlock();

#if PROFILE_ENABLED
			const auto duration = time::ToFloat(time::TStopWatch::now() - startTime);
			if (unlikely(timeOutSec > 0.0f && duration > timeOutSec))
			{
				using namespace StringUtil;
				static const auto name = ToCompactClassName(__PRETTY_FUNCTION__);
				auto log = Logger::Get(name);
				log.OutWarning([&, this](auto& ls)
				{
					ls << "[file: " << srcLoc.file_name() << '(' << srcLoc.line() << ':' << srcLoc.column() << ") "
					   << srcLoc.function_name() << ": TimedOut! " << duration << " sec exceeds its timelimt "
					   << timeOutSec;
				});
			}
#endif // PROFILE_ENABLED
		}
	};
} // namespace hbe
