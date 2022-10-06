// Created by mooming.go@gmail.com, 2022

#include "TaskStream.h"

#include "Engine.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"


namespace HE
{

TaskStream::Request::Request()
    : task(nullptr)
    , start(0)
    , end(0)
{
}

TaskStream::Request::Request(Task& task, TIndex start, TIndex end)
    : task(&task)
    , start(start)
    , end(end)
{
}

TaskStream::TaskStream()
    : threadIndex(-1)
{
    Assert(threadID == std::thread::id());
}

TaskStream::TaskStream(int index, StaticString name)
    : name(name)
    , threadIndex(index)
{
    Assert(threadID == std::thread::id());
    
    auto log = Logger::Get(name);
    log.Out([name=name](auto& ls)
    {
        ls << name.c_str() << " is created.";
    });
}

void TaskStream::InitiateFromCurrentThread()
{
    threadID = std::this_thread::get_id();

    auto& engine = Engine::Get();
    auto& taskSys = engine.GetTaskSystem();

    threadIndex = taskSys.GetThreadIndex(threadID);
    name = taskSys.GetThreadName(threadIndex);
}


void TaskStream::Start()
{
    if (unlikely(threadID != ThreadID()))
    {
        auto log = Logger::Get(name);
        log.OutFatalError([](auto& ls)
        {
            ls << "This TaskStream has its own thread already.";
        });

        return;
    }

    auto& engine = Engine::Get();
    auto& taskSys = engine.GetTaskSystem();

    threadID = taskSys.SetThread(threadIndex, name, [this]()
    {
        auto log = Logger::Get(name);
        log.Out([name=name](auto& ls)
        {
            ls << name.c_str() << " has begun.";
        });

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        while(likely(taskSys.IsRunning()))
        {
            Flush();
        }

        log.Out([name=name](auto& ls)
        {
            ls << name.c_str() << " has been terminated.";
        });
    });
}

void TaskStream::Request(Task& task, TIndex start, TIndex end)
{
    std::lock_guard<std::mutex> lock(queueLock);
    requests.emplace_back(task, start, end);
    task.IncNumStreams();
}

void TaskStream::Flush()
{
    auto log = Logger::Get(name);

    if (unlikely(threadID != std::this_thread::get_id()))
    {
        log.OutFatalError([](auto& ls)
        {
            ls << "Flush is requested from the incorrect thread.";
        });

        return;
    }

    {
        std::lock_guard<std::mutex> lock(queueLock);
        buffer.reserve(requests.size());
        std::swap(requests, buffer);
    }

    if (buffer.size() <= 0)
        return;

    for (auto& request : buffer)
    {
        auto task = request.task;
        if (unlikely(task == nullptr))
        {
            log.OutError([name=name](auto& ls)
            {
                ls << name.c_str() << " Task func is null.";
            });

            continue;
        }

        task->Run(request.start, request.end);
    }

    buffer.clear();
}

} // HE
