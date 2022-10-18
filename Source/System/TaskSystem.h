// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Task.h"
#include "TaskHandle.h"
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
    using TIndex = TaskHandle::TIndex;
    using TaskIndex = Task::TIndex;
    using TKey = TaskHandle::TKey;
    static constexpr TKey InvalidKey = TaskHandle::InvalidKey;
    
private:
    struct TaskSlot final
    {
        TKey key = 0;
        Task task;
    };

    std::atomic<bool> isRunning;
    
    const StaticString name;
    const TIndex numHardwareThreads;
    TIndex workerIndexStart;
    TIndex numWorkers;

    TKey keySeed;
    const ThreadID mainTaskThreadID;
    ThreadID ioTaskThreadID;

    std::mutex requestLock;
    Array<TaskSlot> slots;
    Array<TaskStream> streams;

public:
    static TIndex GetNumHardwareThreads();

    TaskSystem();
    ~TaskSystem();

    void Initialize();
    void Shutdown();
    
    void PreUpdate();
    void Update();
    void PostUpdate();

    inline StaticString GetName() const { return name; }
    inline auto& IsRunning() const { return isRunning; }

public:
    bool IsMainThread() const;
    bool IsIOTaskThread() const;

    inline TIndex GetMainTaskStreamIndex() const { return 0; }
    inline TIndex GetIOTaskStreamIndex() const { return 1; }

    inline auto& GetMainTaskStream() { return streams[GetMainTaskStreamIndex()]; }
    inline auto& GetMainTaskStream() const { return streams[GetMainTaskStreamIndex()]; }
    inline auto& GetIOTaskStream() { return streams[GetIOTaskStreamIndex()]; }
    inline auto& GetIOTaskStream() const { return streams[GetIOTaskStreamIndex()]; }

    inline auto GetWorkerIndexStart() const { return workerIndexStart; }
    inline auto GetNumWorkers() const { return numWorkers; }

public:
    StaticString GetCurrentStreamName() const;
    StaticString GetStreamName(int index) const;
    TIndex GetCurrentStreamIndex() const;
    TIndex GetStreamIndex(ThreadID id) const;

public:
    Task* GetTask(TKey key, TIndex taskIndex);
    const Task* GetTask(TKey key, TIndex taskIndex) const;

    TaskHandle RegisterTask(TIndex streamIndex, StaticString taskName, Runnable func);
    TaskHandle DispatchTask(StaticString taskName, Runnable func, TIndex streamIndex);
    TaskHandle DispatchTask(StaticString taskName, TaskIndex size, Runnable func);
    TaskHandle DispatchTask(StaticString taskName, TaskIndex size, Runnable func, TIndex numStreams);

private:
    void DeregisterTask(TIndex streamIndex, TKey key);
    void DeregisterTaskAsync(TIndex streamIndex, TKey key);
    void ReleaseTask(TKey key, TIndex slotIndex);

    void BuildStreams();
    TKey IssueTaskKey();
    void Flush();
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
