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
}

TaskStream::TaskStream(int index, StaticString name)
    : name(name)
    , threadIndex(index)
{
    auto log = Logger::Get(name);
    log.Out([name=name](auto& ls)
    {
        ls << name.c_str() << " is created.";
    });
}

void TaskStream::Start()
{
    auto& engine = Engine::Get();
    auto& taskSys = engine.GetTaskSystem();

    taskSys.SetThread(threadIndex, name, [this]()
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
            {
                std::lock_guard<std::mutex> lock(queueLock);
                std::swap(requests, buffer);
            }

            if (buffer.size() <= 0)
                continue;

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

} // HE
