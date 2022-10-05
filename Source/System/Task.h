// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Runnable.h"
#include "String/StaticString.h"
#include <atomic>


namespace HE
{

class Task final
{
public:
    using TIndex = std::size_t;

private:
    StaticString name;

    uint8_t numStreams;
    std::atomic<uint8_t> numDone;

    TIndex size;
    Runnable func;

public:
    Task();
    Task(StaticString name, TIndex size, Runnable func);
    ~Task() = default;

    void Run();
    void Run(TIndex start, TIndex end);

    void BusyWait() const;
    void Wait(uint32_t intervalMilliSecs = 10) const;

    inline int NumDone() const { return numDone.load(); }
    inline int NumStreams() const { return numStreams; }

    inline bool IsDone() const { return numDone >= numStreams; }
    inline void IncNumStreams() { ++numStreams; }
    inline auto GetSize() const { return size; }
};

} // HE
