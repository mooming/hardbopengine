// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AllocStats.h"

#include <new>
#include "../Engine/Engine.h"
#include "Log/Logger.h"
#include "OSAL/OSDebug.h"


namespace hbe
{

	AllocStats::AllocStats() :
		name(""), isInline(false), capacity(0), usage(0), maxUsage(0), totalRequested(0), maxRequested(0),
		totalFallback(0), maxFallback(0), allocCount(0), deallocCount(0), fallbackCount(0)
	{}

	void AllocStats::onRegister(const char* inName, bool inIsInline, size_t inCapacity) noexcept
	{
		name = StaticString(inName);

		isInline = inIsInline;
		capacity = inCapacity;
		usage = 0;
		maxUsage = 0;

		totalRequested = 0;
		maxRequested = 0;
		totalFallback = 0;
		maxFallback = 0;

		allocCount = 0;
		deallocCount = 0;
		fallbackCount = 0;
	}

	void AllocStats::reset() noexcept
	{
		this->~AllocStats();
		new (this) AllocStats();
	}

	void AllocStats::report() noexcept
	{
#if PROFILE_ENABLED
		auto& engine = Engine::get();
		auto& stats = engine.getStatistics();
		stats.report(*this);
#endif // PROFILE_ENABLED
	}

	void AllocStats::print() noexcept
	{
		static const StaticString moduleName("AllocStats");
		auto log = Logger::get(moduleName, ELogLevel::Verbose);

		log.out([this](auto& ls) { ls << "name = " << name; });

		log.out([this](auto& ls) { ls << "inline = " << isInline; });

		log.out([this](auto& ls) { ls << "usage = " << usage << " / " << maxUsage << " / " << capacity; });

		log.out([this](auto& ls)
		{
			ls << "requested = " << (allocCount > 0 ? totalRequested / allocCount : totalRequested) << " / "
			   << maxRequested << " / " << totalRequested;
		});

		log.out([this](auto& ls)
		{
			ls << "fallback = " << (fallbackCount > 0 ? totalFallback / fallbackCount : totalFallback) << " / "
			   << maxFallback << " / " << totalFallback;
		});

		log.out([this](auto& ls)
		{
			ls << "alloc/dealloc/fallback = " << allocCount << " / " << deallocCount << " / " << fallbackCount << '('
			   << (allocCount > 0 ? fallbackCount * 100 / allocCount : 0) << ')';
		});
	}

} // namespace hbe
