// Created by mooming.go@gmail.com, 2022

#include "TaskStream.h"

#include "Engine.h"
#include "TaskSystem.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSThread.h"


namespace HE
{

TaskStream::Request::Request()
    : task(nullptr)
    , start(0)
    , end(0)
{
}

TaskStream::Request::Request(TKey key, Task& task, TIndex start, TIndex end)
    : key(key)
    , task(&task)
    , start(start)
    , end(end)
{
}

TaskStream::TaskStream()
    : time(Time::GetNow())
    , deltaTime(0.0f)
    , flipCount(0)
    , isResidentListDirty(false)

{
    Assert(threadID == std::thread::id());
}

TaskStream::TaskStream(StaticString name)
    : name(name)
    , time(Time::GetNow())
    , deltaTime(0.0f)
    , flipCount(0)
    , isResidentListDirty(false)
{
    Assert(threadID == std::thread::id());
    
    auto log = Logger::Get(name);
    log.Out([name=name](auto& ls)
    {
        ls << name.c_str() << " is created.";
    });
}

void TaskStream::Flush()
{
    if (unlikely(threadID != std::this_thread::get_id()))
    {
        auto log = Logger::Get(name);
        log.OutFatalError("Incorrect thread!");
        return;
    }

    auto currentTime = Time::GetNow();
    deltaTime = Time::ToFloat(currentTime - time);
    time = currentTime;

    FlipBuffers();

    if (requestsBuffer.size() > 0)
    {
        UpdateRequests();
    }

    if (residentsBuffer.size() > 0)
    {
        UpdateResidents();
    }
}

void TaskStream::Start(TaskSystem& taskSys)
{
    auto func = [this, &taskSys]()
    {
        auto log = Logger::Get(name);
        log.Out([name = name](auto& ls)
        {
            ls << name.c_str() << " has begun.";
        });

        threadID = std::this_thread::get_id();
        RunLoop(taskSys.IsRunning());

        log.Out([name = name](auto& ls)
        {
            ls << name.c_str() << " has been terminated.";
        });
    };

    thread = std::thread(func);
    OS::SetThreadPriority(thread, 0);
    
    threadID = thread.get_id();
}

void TaskStream::Request(TKey key, Task& task, TIndex start, TIndex end)
{
    std::lock_guard<std::mutex> lock(queueLock);
    requests.emplace_back(key, task, start, end);
    task.IncNumStreams();
}

void TaskStream::AddResident(TKey key, Task& task)
{
    std::lock_guard<std::mutex> lock(queueLock);
    residents.emplace_back(key, task, 0, 0);
    isResidentListDirty = true;
}

void TaskStream::RemoveResidentTask(TKey key)
{
    std::lock_guard<std::mutex> lock(queueLock);

    auto predicate = [key](auto& item)
    {
        return item.key == key;
    };

    auto found = std::find_if(residents.begin(), residents.end(), predicate);
    if (found == residents.end())
        return;

    residents.erase(found);
    isResidentListDirty = true;
}

void TaskStream::RemoveResidentTaskSync(TKey key)
{
    uint64_t count = 0;

    {
        std::lock_guard<std::mutex> lock(queueLock);

        count = flipCount;

        auto predicate = [key](auto& item)
        {
            return item.key == key;
        };

        auto found = std::find_if(residents.begin(), residents.end(), predicate);
        if (found == residents.end())
            return;

        residents.erase(found);
        isResidentListDirty = true;
    }

    while (flipCount.load() == count);
}

void TaskStream::FlipBuffers()
{
    std::lock_guard<std::mutex> lock(queueLock);

    ++flipCount;

    {
        const auto size = requests.size();
        requestsBuffer.reserve(size);
        std::swap(requests, requestsBuffer);
    }

    if (unlikely(isResidentListDirty))
    {
        const auto size = residents.size();
        residentsBuffer.reserve(size);
        residentsBuffer = residents;
    }
}

void TaskStream::RunLoop(const std::atomic<bool>& isRunning)
{
    auto log = Logger::Get(name);

    if (unlikely(threadID != std::this_thread::get_id()))
    {
        log.OutFatalError("Incorrect thread!");
        return;
    }

    while(likely(isRunning))
    {
        auto currentTime = Time::GetNow();
        deltaTime = Time::ToFloat(currentTime - time);
        time = currentTime;

        if (unlikely(deltaTime > 0.1f))
        {
            log.OutWarning([this](auto& ls)
            {
                ls << "Slow DeltaTime = " << deltaTime;                
            });
        }

        FlipBuffers();

        if (requestsBuffer.size() > 0)
        {
            UpdateRequests();
        }

        if (residentsBuffer.size() > 0)
        {
            UpdateResidents();
        }
    }
}

void TaskStream::UpdateRequests()
{
    for (auto& request : requestsBuffer)
    {
        auto task = request.task;
        if (unlikely(task == nullptr))
        {
            auto log = Logger::Get(name);
            log.OutError([name=name](auto& ls)
            {
                ls << name.c_str() << " task func is null.";
            });

            continue;
        }

        task->Run(request.start, request.end);
    }

    requestsBuffer.clear();
}

void TaskStream::UpdateResidents()
{
    for (auto& request : residentsBuffer)
    {
        auto task = request.task;
        if (unlikely(task == nullptr))
        {
            auto log = Logger::Get(name);
            log.OutError([name=name](auto& ls)
            {
                ls << name.c_str() << " task func is null.";
            });

            continue;
        }

        task->Run(0, 0);
        task->ClearNumDone();
    }
}

} // HE
