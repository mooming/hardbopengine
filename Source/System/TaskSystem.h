// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Container/Array.h"
#include "Memory/Optional.h"
#include "Task.h"
#include "TaskHandle.h"
#include "TaskStream.h"
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
        static constexpr TIndex MainStreamIndex = 0;
        static constexpr TIndex IOStreamIndex = 1;

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
        static void SetThreadName(StaticString name);
        static void SetStreamIndex(TIndex index);

        static bool IsMainThread();
        static bool IsIOThread();

    public:
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
        inline TIndex GetMainTaskStreamIndex() const { return MainStreamIndex; }
        inline TIndex GetIOTaskStreamIndex() const { return IOStreamIndex; }

        inline auto& GetMainTaskStream()
        {
            return streams[GetMainTaskStreamIndex()];
        }
        inline auto& GetMainTaskStream() const
        {
            return streams[GetMainTaskStreamIndex()];
        }
        inline auto& GetIOTaskStream()
        {
            return streams[GetIOTaskStreamIndex()];
        }
        inline auto& GetIOTaskStream() const
        {
            return streams[GetIOTaskStreamIndex()];
        }

        inline auto GetWorkerIndexStart() const { return workerIndexStart; }
        inline auto GetNumWorkers() const { return numWorkers; }
        inline StaticString GetCurrentStreamName() const
        {
            return GetCurrentThreadName();
        }

    public:
        StaticString GetCurrentThreadName() const;
        StaticString GetStreamName(int index) const;

        TIndex GetCurrentStreamIndex() const;
        TIndex GetStreamIndex(ThreadID id) const;

    public:
        Task* GetTask(TKey key, TIndex taskIndex);
        const Task* GetTask(TKey key, TIndex taskIndex) const;

        TaskHandle RegisterTask(
            TIndex streamIndex, StaticString taskName, Runnable func);
        TaskHandle DispatchTask(
            StaticString taskName, Runnable func, TIndex streamIndex);
        TaskHandle DispatchTask(
            StaticString taskName, TaskIndex size, Runnable func);
        TaskHandle DispatchTask(StaticString taskName, TaskIndex size,
            Runnable func, TIndex numStreams);

    private:
        void DeregisterTask(TIndex streamIndex, TKey key);
        void DeregisterTaskAsync(TIndex streamIndex, TKey key);
        void ReleaseTask(TKey key, TIndex slotIndex);

        void BuildStreams();
        TKey IssueTaskKey();
        void Flush();
    };

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

    class TaskSystemTest : public TestCollection
    {
    public:
        TaskSystemTest()
            : TestCollection("TaskSystemTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };

} // namespace HE
#endif //__UNIT_TEST__
