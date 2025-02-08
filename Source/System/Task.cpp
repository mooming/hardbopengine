// Created by mooming.go@gmail.com, 2022

#include "Task.h"

#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "Time.h"

namespace HE
{

    Task::Task()
        : numStreams(0),
          numDone(0),
          isCancelled(false),
          size(0),
          func(nullptr)
    {
    }

    Task::Task(StaticString name, size_t size, Runnable func)
        : name(name),
          numStreams(0),
          numDone(0),
          isCancelled(false),
          size(size),
          func(func)
    {
    }

    void Task::Reset()
    {
        name = StaticString();
        threadID = TThreadID();

        numStreams = 0;
        numDone.store(0, std::memory_order_relaxed);
        isCancelled.store(false, std::memory_order_relaxed);
        size = 0;
        func = nullptr;
    }

    void Task::Reset(StaticString inName, TIndex inSize, Runnable inFunc)
    {
        name = inName;
        threadID = TThreadID();

        numStreams = 0;
        numDone.store(0, std::memory_order_relaxed);
        isCancelled.store(false, std::memory_order_relaxed);
        size = inSize;
        func = inFunc;
    }

    void Task::Run()
    {
        if (unlikely(IsCancelled()))
        {
            return;
        }

        if (unlikely(IsDone()))
        {
            return;
        }

        if (unlikely(func == nullptr))
        {
            auto log = Logger::Get(name);
            log.OutFatalError("The given runnable is null.");
            numDone.store(numStreams, std::memory_order_relaxed);

            return;
        }

        func(0, size);

        numDone.fetch_add(1, std::memory_order_relaxed);
    }

    void Task::Run(TIndex start, TIndex end)
    {
        if (unlikely(IsCancelled()))
        {
            return;
        }

        if (unlikely(IsDone()))
        {
            return;
        }

        if (unlikely(func == nullptr))
        {
            auto log = Logger::Get(name);
            log.OutFatalError("The given runnable is null.");
            numDone.store(numStreams, std::memory_order_relaxed);

            return;
        }

        func(start, end);

        numDone.fetch_add(1, std::memory_order_relaxed);
    }

    void Task::ForceRun()
    {
        if (unlikely(func == nullptr))
        {
            auto log = Logger::Get(name);
            log.OutFatalError("The given runnable is null.");
            numDone.store(numStreams, std::memory_order_relaxed);

            return;
        }

        func(0, size);
    }

    void Task::Cancel()
    {
        auto log = Logger::Get(name);
        log.Out("The task is cancelled.");

        isCancelled.store(true, std::memory_order_relaxed);
    }

    void Task::BusyWait() const
    {
        while (!IsDone())
            ;
    }

    void Task::Wait(uint32_t intervalMilliSecs) const
    {
        const auto interval = std::chrono::milliseconds(intervalMilliSecs);

        while (!IsDone())
        {
            std::this_thread::sleep_for(interval);
        }
    }

} // namespace HE
