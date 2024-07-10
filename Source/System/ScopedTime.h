// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Time.h"


namespace HE
{
namespace Time
{

class ScopedTime final
{
  private:
    TDuration& duration;
    TTime start;

  public:
    ScopedTime(const ScopedTime&) = delete;

    ScopedTime(TDuration& outDeltaTime) : duration(outDeltaTime), start(TStopWatch::now()) {}

    ~ScopedTime()
    {
        TTime end = TStopWatch::now();
        duration = end - start;
    }
};

} // namespace Time
} // namespace HE
