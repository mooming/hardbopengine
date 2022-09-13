// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Types.h"
#include <chrono>


namespace HE
{
namespace Time
{
using MilliSec = uint64_t;
using TStopWatch = std::chrono::steady_clock;
using TTime = std::chrono::time_point<TStopWatch>;
using TDuration = TStopWatch::duration;

template <typename T = float>
constexpr T ToMilliSec(TDuration duration)
{
    static_assert(std::is_floating_point<T>());
    std::chrono::duration<T, std::milli> milliSecs = duration;
    return milliSecs.count();
}

class Measure
{
private:
    TDuration& duration;
    TTime start;
    
public:
    Measure(TDuration& outDeltaTime)
    : duration(outDeltaTime)
    , start(TStopWatch::now())
    {
    }
    
    ~Measure()
    {
        TTime end = TStopWatch::now();
        duration = end - start;
    }
};

void Sleep(MilliSec milli);

} // Time
} // HE
