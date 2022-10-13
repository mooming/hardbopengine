// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Time.h"
#include <atomic>
#include <cstdint>


namespace HE
{

class StaticString;

class SystemStatistics final
{
private:
    std::atomic<uint64_t> frameCount;
    std::atomic<uint64_t> slowFrameCount;
    Time::TTime startTime;
    Time::TTime currentTime;
    double timeSinceStart;
    float deltaTime;

public:
    SystemStatistics();
    ~SystemStatistics() = default;

    const StaticString& GetName() const;
    void UpdateCurrentTime();
    void Print();

    inline void IncFrameCount() { ++frameCount;}
    inline void IncSlowFrameCount() { ++slowFrameCount; }
    inline auto GetStartTime() const { return startTime; }
    inline auto GetCurrentTime() const { return currentTime; }
    inline auto GetTimeSinceStart() const { return timeSinceStart; }
    inline auto GetDeltaTime() const { return deltaTime; }
};

} // HE
