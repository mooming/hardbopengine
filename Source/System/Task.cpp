// Created by mooming.go@gmail.com, 2022

#include "Task.h"

#include "Time.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"


namespace HE
{
Task::Task()
    : numStreams(0)
    , numDone(0)
    , size(0)
    , func(nullptr)
{
}

Task::Task(StaticString name, size_t size, Runnable func)
    : name(name)
    , numStreams(0)
    , numDone(0)
    , size(size)
    , func(func)
{
}

void Task::Run()
{
    func(0, size);
    ++numDone;
}

void Task::Run(TIndex start, TIndex end)
{
    if (unlikely(func == nullptr))
    {
        auto log = Logger::Get(name);
        log.OutWarning("Runnable is null.");
        return;
    }

    func(start, end);
    ++numDone;
}

void Task::BusyWait() const
{
    while(numDone < numStreams);
}

void Task::Wait(uint32_t intervalMilliSecs) const
{
    const auto interval = std::chrono::milliseconds(intervalMilliSecs);

    while(numDone < numStreams)
    {
        std::this_thread::sleep_for(interval);
    }
}

} // HE
