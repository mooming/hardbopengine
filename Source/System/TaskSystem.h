// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Task.h"
#include "TaskStream.h"
#include "Container/Array.h"
#include "Memory/Optional.h"
#include <array>
#include <atomic>
#include <thread>


namespace HE
{

class TaskSystem final
{
public:
    using Thread = std::thread;
    using ThreadID = std::thread::id;
    using TIndex = Array<int>::TIndex;
    using TaskIndex = Task::TIndex;
    using TKey = uint32_t;
    static constexpr TKey InvalidKey = 0;

    struct TaskHandle final
    {
        TKey key = 0;
        TIndex index = 0;
        Optional<Task&> task;

        bool IsValid() const;
        void Wait(uint32_t intervalMilliSecs);
        void BusyWait();
    };

private:
    struct TaskSlot final
    {
        TKey key = 0;
        Optional<Task> task;
    };

    const StaticString name;
    const TIndex numHardwareThreads;
    TIndex numWorkerThreads;
    
    uint64_t keySeed;
    TIndex workerIndexStart;
    TIndex numWorkers;

    std::atomic<bool> isRunning;
    Array<StaticString> threadNames;
    Array<std::thread> threads;

    Array<TaskSlot> slots;
    Array<TaskStream> streams;

public:
    TaskSystem();
    ~TaskSystem();

    void Initialize();
    void Shutdown();
    
    void PreUpdate();
    void Update();
    void PostUpdate();

    StaticString GetCurrentThreadName() const;
    StaticString GetThreadName(int index) const;
    ThreadID SetThread(TIndex index, StaticString name, std::function<void()> func);

    TIndex GetCurrentThreadIndex() const;
    TIndex GetThreadIndex(ThreadID id) const;

    TaskHandle AllocateTask(TIndex index, Runnable func);
    TaskHandle AllocateTask(StaticString name, TaskIndex size, Runnable func);
    TaskHandle AllocateTask(StaticString name, TaskIndex size, Runnable func, TIndex numStreams);
    void DeallocateTask(TaskHandle&& task);

    void FlushDone();

    inline StaticString GetName() const { return name; }
    inline bool IsRunning() const { return isRunning.load(); }

    inline TIndex GetMainTaskStreamIndex() const { return 0; }
    inline TIndex GetIOTaskStreamIndex() const { return 1; }

    inline auto& GetMainTaskStream() { return streams[GetMainTaskStreamIndex()]; }
    inline auto& GetMainTaskStream() const { return streams[GetMainTaskStreamIndex()]; }
    inline auto& GetIOTaskStream() { return streams[GetIOTaskStreamIndex()]; }
    inline auto& GetIOTaskStream() const { return streams[GetIOTaskStreamIndex()]; }

private:
    void BuildStreams();
    void SetCPUAffinities();
    TKey IssueTaskKey();
};

} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class TaskSystemTest : public TestCollection
{
public:
    TaskSystemTest() : TestCollection("TaskSystemTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
