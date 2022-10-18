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
    , isDone(false)
    , size(0)
    , func(nullptr)
{
}

Task::Task(StaticString name, size_t size, Runnable func)
    : name(name)
    , numStreams(0)
    , numDone(0)
    , isDone(false)
    , size(size)
    , func(func)
{
}

void Task::Reset()
{
    this->~Task();
    new (this) Task();
}

void Task::Reset(StaticString name, TIndex size, Runnable func)
{
    this->~Task();
    new (this) Task(name, size, func);
}

void Task::Run()
{
    func(0, size);

    if (isDone)
        return;

    ++numDone;

    if (numDone >= numStreams)
    {
        isDone = true;
    }
}

void Task::Run(TIndex start, TIndex end)
{
    if (unlikely(func == nullptr))
    {
        auto log = Logger::Get(name);
        log.OutWarning("Runnable is null.");
        ++numDone;
        return;
    }

    func(start, end);

    if (isDone)
        return;

    ++numDone;

    if (numDone >= numStreams)
    {
        isDone = true;
    }
}

void Task::BusyWait() const
{
    while(!isDone);
}

void Task::Wait(uint32_t intervalMilliSecs) const
{
    const auto interval = std::chrono::milliseconds(intervalMilliSecs);

    while(!isDone)
    {
        std::this_thread::sleep_for(interval);
    }
}

} // HE
