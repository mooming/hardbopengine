// Created by mooming.go@gmail.com

#include "AllocStats.h"

#include "../Engine/Engine.h"
#include "Log/Logger.h"
#include "OSAL/OSDebug.h"
#include <new>

namespace HE
{

AllocStats::AllocStats()
    : name(""),
      isInline(false),
      capacity(0),
      usage(0),
      maxUsage(0),
      totalRequested(0),
      maxRequested(0),
      totalFallback(0),
      maxFallback(0),
      allocCount(0),
      deallocCount(0),
      fallbackCount(0)
{
}

void AllocStats::OnRegister(
    const char* inName, bool inIsInnline, size_t inCapacity)
{
    name = StaticString(inName);

    isInline = inIsInnline;
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

void AllocStats::Reset()
{
    this->~AllocStats();
    new (this) AllocStats();
}

void AllocStats::Report()
{
#ifdef PROFILE_ENABLED
    auto& engine = Engine::Get();
    auto& stats = engine.GetStatistics();
    stats.Report(*this);
#endif // PROFILE_ENABLED
}

void AllocStats::Print()
{
    static const StaticString moduleName("AllocStats");
    auto log = Logger::Get(moduleName, ELogLevel::Verbose);

    log.Out([this](auto& ls) { ls << "name = " << name; });

    log.Out([this](auto& ls) { ls << "inline = " << isInline; });

    log.Out([this](auto& ls) {
        ls << "usage = " << usage << " / " << maxUsage << " / " << capacity;
    });

    log.Out([this](auto& ls) {
        ls << "requested = "
           << (allocCount > 0 ? totalRequested / allocCount : totalRequested)
           << " / " << maxRequested << " / " << totalRequested;
    });

    log.Out([this](auto& ls) {
        ls << "fallback = "
           << (fallbackCount > 0 ? totalFallback / fallbackCount
                                 : totalFallback)
           << " / " << maxFallback << " / " << totalFallback;
    });

    log.Out([this](auto& ls) {
        ls << "alloc/dealloc/fallback = " << allocCount << " / " << deallocCount
           << " / " << fallbackCount << '('
           << (allocCount > 0 ? fallbackCount * 100 / allocCount : 0) << ')';
    });
}

} // namespace HE
