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
    std::atomic<bool> isDone;

    TIndex size;
    Runnable func;

public:
    Task();
    Task(StaticString name, TIndex size, Runnable func);
    ~Task() = default;

    void Reset();
    void Reset(StaticString name, TIndex size, Runnable func);
    
    void Run();
    void Run(TIndex start, TIndex end);

    void BusyWait() const;
    void Wait(uint32_t intervalMilliSecs = 10) const;

    inline auto GetName() const { return name; }

    inline int NumDone() const { return numDone.load(); }
    inline int NumStreams() const { return numStreams; }
    inline void ClearNumDone() { numDone = 0; }

    inline bool IsDone() const { return isDone; }
    inline void IncNumStreams() { ++numStreams; }
    inline auto GetSize() const { return size; }
};

} // HE
