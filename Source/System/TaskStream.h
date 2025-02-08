// Created by mooming.go@gmail.com, 2022

#pragma once

#include "HSTL/HVector.h"
#include "String/StaticString.h"
#include "Task.h"
#include "TaskHandle.h"
#include "Time.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace HE
{

    class TaskSystem;

    class TaskStream final
    {
        template <typename T>
        using TVector = HSTL::HVector<T>;
        using TIndex = Task::TIndex;
        using ThreadID = std::thread::id;
        using TKey = TaskHandle::TKey;

        struct Request final
        {
            TKey key;
            Task *task;
            TIndex start;
            TIndex end;

            Request();
            Request(TKey key, Task &task, TIndex start, TIndex end);
            ~Request() = default;
        };

        using TRequests = TVector<Request>;

    private:
        StaticString name;
        ThreadID threadID;

        std::mutex queueLock;
        std::mutex cvLock;
        std::condition_variable cv;
        std::thread thread;

        float deltaTime;
        std::atomic<uint64_t> flipCount;
        std::atomic<bool> isDirty;
        bool isResidentListDirty;
        bool hasCancelledTask;

        TRequests newRequests;
        TRequests newResidents;

        TRequests requests;
        TRequests residents;

    public:
        TaskStream();
        TaskStream(StaticString name);
        ~TaskStream() = default;

        void WakeUp();
        void Flush();

        inline auto GetName() const { return name; }
        inline auto GetThreadID() const { return threadID; }
        inline auto &GetThread() { return thread; }
        inline auto &GetThread() const { return thread; }
        inline auto GetFlipCount() const { return flipCount.load(); }

    private:
        void Start(TaskSystem &taskSys, TaskHandle::TIndex streamIndex);

        void Request(TKey key, Task &task, TIndex start, TIndex end);
        void AddResident(TKey key, Task &task);
        void RemoveResidentTask(TKey key);
        void RemoveResidentTaskSync(TKey key);

        void FlipBuffers();
        void RunLoop(const std::atomic<bool> &isRunning);
        void UpdateRequests();
        void UpdateResidents();

        friend class TaskSystem;
    };

} // namespace HE
