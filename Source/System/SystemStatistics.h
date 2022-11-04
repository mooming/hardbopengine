// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Time.h"
#include "Config/BuildConfig.h"
#include <atomic>
#include <cstdint>
#include <vector>


namespace std
{
struct source_location;
} // std

namespace HE
{

class StaticString;

class SystemStatistics final
{
public:
#ifdef PROFILE_ENABLED
    struct AllocProfile final
    {
        const char* allocatorName;
        const char* file;
        const char* func;
        size_t lineNumber;
        size_t columnNumber;
        size_t maxUsage;

        AllocProfile();
        AllocProfile(const char* allocatorName
            , const char* file, const char* func
            , size_t lineNumber
            , size_t columnNumber, size_t maxUsage);
        ~AllocProfile() = default;
    };
#endif // PROFILE_ENABLED

private:
    static_assert(std::atomic<uint64_t>::is_always_lock_free, "");
    std::atomic<uint64_t> frameCount;
    std::atomic<uint64_t> slowFrameCount;
    std::atomic<uint64_t> engineLogCount;
    std::atomic<uint64_t> logCount;
    std::atomic<uint64_t> longLogCount;

    Time::TTime startTime;
    Time::TTime currentTime;
    double timeSinceStart;
    float deltaTime;

#ifdef PROFILE_ENABLED
    std::vector<AllocProfile> allocatorProfiles;
#endif // PROFILE_ENABLED
    
public:
    SystemStatistics();
    ~SystemStatistics() = default;

    const StaticString& GetName() const;
    void UpdateCurrentTime();

#ifdef PROFILE_ENABLED
    void Report(const char* allocatorName
        , const std::source_location& location, size_t maxUsage);
#endif // PROFILE_ENABLED

    void Print();
    void PrintAllocatorProfiles();

    inline void IncFrameCount() { frameCount.fetch_add(1, std::memory_order_relaxed);}
    inline void IncSlowFrameCount() { slowFrameCount.fetch_add(1, std::memory_order_relaxed); }
    inline void IncEngineLogCount() { engineLogCount.fetch_add(1, std::memory_order_relaxed); }
    inline void IncLogCount() { logCount.fetch_add(1, std::memory_order_relaxed); }
    inline void IncLongLogCount() { longLogCount.fetch_add(1, std::memory_order_relaxed); }
    
    inline auto GetStartTime() const { return startTime; }
    inline auto GetCurrentTime() const { return currentTime; }
    inline auto GetTimeSinceStart() const { return timeSinceStart; }
    inline auto GetDeltaTime() const { return deltaTime; }
};

} // HE
