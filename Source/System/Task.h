// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Runnable.h"
#include "String/StaticString.h"
#include <atomic>
#include <thread>


namespace HE
{

class Task final
{
  public:
    using TIndex = std::size_t;
    using TThreadID = std::thread::id;

  private:
    StaticString name;
    TThreadID threadID;

    uint8_t numStreams;
    std::atomic<uint8_t> numDone;
    std::atomic<bool> isCancelled;

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
    void ForceRun();

    void Cancel();
    void BusyWait() const;
    void Wait(uint32_t intervalMilliSecs = 10) const;

    inline auto GetName() const { return name; }
    inline auto& GetThreadID() const { return threadID; }
    inline bool IsCurrentThread() const { return threadID == std::this_thread::get_id(); }
    inline void SetThreadID(const TThreadID& id) { threadID = id; }

    inline bool IsDone() const { return NumDone() >= numStreams; }
    inline bool IsCancelled() const { return isCancelled.load(std::memory_order_relaxed); }

    inline int NumStreams() const { return numStreams; }
    inline int NumDone() const { return numDone.load(std::memory_order_relaxed); }
    inline void IncNumStreams() { ++numStreams; }
    inline void SetDone() { numDone.store(numStreams, std::memory_order_relaxed); }
    inline void ClearDone() { numDone.store(0, std::memory_order_relaxed); }

    inline auto GetSize() const { return size; }
};

} // namespace HE
