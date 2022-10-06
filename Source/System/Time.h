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
using TMilliSecs = std::chrono::milliseconds;


inline TTime GetNow()
{
    return std::chrono::steady_clock::now();
}

inline float ToFloat(TStopWatch::duration duration)
{
    std::chrono::duration<float> delta = duration;
    return delta.count();
}

inline TMilliSecs::rep ToMilliSeconds(TStopWatch::duration duration)
{
    auto delta = std::chrono::duration_cast<TMilliSecs>(duration);
    return delta.count();
}

class Measure final
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
