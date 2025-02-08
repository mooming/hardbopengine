// Created by mooming.go@gmail.com, 2022

#include "TaskStream.h"

#include "Engine.h"
#include "Log/Logger.h"
#include "OSAL/Intrinsic.h"
#include "OSAL/OSThread.h"
#include "System/ScopedLock.h"
#include "TaskSystem.h"
#include <thread>

namespace HE
{

    TaskStream::Request::Request()
        : task(nullptr),
          start(0),
          end(0)
    {
    }

    TaskStream::Request::Request(TKey key, Task& task, TIndex start, TIndex end)
        : key(key),
          task(&task),
          start(start),
          end(end)
    {
    }

    TaskStream::TaskStream()
        : deltaTime(0.0f),
          flipCount(0),
          isDirty(false),
          isResidentListDirty(false),
          hasCancelledTask(false)

    {
        Assert(threadID == std::thread::id());
    }

    TaskStream::TaskStream(StaticString name)
        : name(name),
          deltaTime(0.0f),
          flipCount(0),
          isDirty(false),
          isResidentListDirty(false),
          hasCancelledTask(false)
    {
        Assert(threadID == std::thread::id());

        auto log = Logger::Get(name);
        log.Out(
            [name = name](auto& ls) { ls << name.c_str() << " is created."; });
    }

    void TaskStream::WakeUp()
    {
        cv.notify_one();
    }

    void TaskStream::Flush()
    {
        if (unlikely(threadID != std::this_thread::get_id()))
        {
            auto log = Logger::Get(name);
            log.OutFatalError("Incorrect thread!");
            return;
        }

        FlipBuffers();

        auto startTime = Time::GetNow();

        if (requests.size() > 0)
        {
            UpdateRequests();
        }

        if (residents.size() > 0)
        {
            UpdateResidents();
        }

        auto endTime = Time::GetNow();
        deltaTime = Time::ToFloat(endTime - startTime);

        if (unlikely(deltaTime > 0.1f))
        {
            auto log = Logger::Get(name);
            log.OutWarning(
                [this](auto& ls) { ls << "Slow DeltaTime = " << deltaTime; });
        }
    }

    void TaskStream::Start(TaskSystem& taskSys, TaskHandle::TIndex streamIndex)
    {
        auto func = [this, &taskSys, streamIndex]() {
            TaskSystem::SetThreadName(name);
            TaskSystem::SetStreamIndex(streamIndex);

            auto log = Logger::Get(name);
            log.Out([name = name](
                        auto& ls) { ls << name.c_str() << " has begun."; });

            threadID = std::this_thread::get_id();
            RunLoop(taskSys.IsRunning());

            log.Out([name = name](auto& ls) {
                ls << name.c_str() << " has been terminated.";
            });
        };

        thread = std::thread(func);
        OS::SetThreadPriority(thread, 0);

        threadID = thread.get_id();
    }

    void TaskStream::Request(TKey key, Task& task, TIndex start, TIndex end)
    {
        {
            ScopedLock lock(queueLock);

            task.SetThreadID(threadID);
            task.IncNumStreams();

            newRequests.emplace_back(key, task, start, end);

            auto log = Logger::Get(name);
            log.Out([&](auto& ls) {
                ls << "Request: Key = " << key << ", Task = " << task.GetName()
                   << '[' << start << ", " << end << ')';
            });

            isDirty.store(true, std::memory_order_release);
        }

        cv.notify_one();
    }

    void TaskStream::AddResident(TKey key, Task& task)
    {
        {
            ScopedLock lock(queueLock);

            task.SetThreadID(threadID);
            task.IncNumStreams();

            newResidents.emplace_back(key, task, 0, 0);

            auto log = Logger::Get(name);
            log.Out([&](auto& ls) {
                ls << "AddResident: Key = " << key
                   << ", Task = " << task.GetName();
            });

            isResidentListDirty = true;
            isDirty.store(true, std::memory_order_release);
        }

        cv.notify_one();
    }

    void TaskStream::RemoveResidentTask(TKey key)
    {
        {
            ScopedLock lock(queueLock);

            auto predicate = [key](auto& item) { return item.key == key; };

            auto found =
                std::find_if(residents.begin(), residents.end(), predicate);
            if (found == residents.end())
            {
                return;
            }

            auto log = Logger::Get(name);
            auto task = found->task;
            if (likely(task != nullptr))
            {
                log.Out([&](auto& ls) {
                    ls << "RemoveResidentTask: Key = " << key
                       << ", Task = " << task->GetName() << '[' << found->start
                       << ", " << found->end << ')';
                });

                task->SetDone();
                task->Cancel();
            }
            else
            {
                log.Out([&](auto& ls) {
                    ls << "RemoveResident: Key = " << key << ", Task = Null"
                       << '[' << found->start << ", " << found->end << ')';
                });
            }

            isResidentListDirty = true;
            hasCancelledTask = true;
            isDirty.store(true, std::memory_order_release);
        }

        cv.notify_one();
    }

    void TaskStream::RemoveResidentTaskSync(TKey key)
    {
        uint64_t count = 0;

        {
            ScopedLock lock(queueLock);

            count = flipCount.load(std::memory_order_relaxed);

            auto predicate = [key](auto& item) { return item.key == key; };

            auto found =
                std::find_if(residents.begin(), residents.end(), predicate);
            if (found == residents.end())
            {
                return;
            }

            auto log = Logger::Get(name);
            auto task = found->task;

            if (likely(task != nullptr))
            {
                log.Out([&](auto& ls) {
                    ls << "RemoveResidentTaskSync: Key = " << key
                       << ", Task = " << task->GetName() << '[' << found->start
                       << ", " << found->end << ')';
                });

                task->SetDone();
                task->Cancel();
            }
            else
            {
                log.Out([&](auto& ls) {
                    ls << "RemoveResidentSyncSync: Key = " << key
                       << ", Task = Null" << '[' << found->start << ", "
                       << found->end << ')';
                });
            }

            isResidentListDirty = true;
            hasCancelledTask = true;

            isDirty.store(true, std::memory_order_release);
        }

        {
            std::unique_lock lock(cvLock);
            cv.notify_one();
        }

        while (flipCount.load(std::memory_order_relaxed) == count)
            ;
    }

    void TaskStream::FlipBuffers()
    {
        ScopedLock lock(queueLock);
        isDirty.store(false, std::memory_order_release);

        {
            const auto size = newRequests.size();
            requests.reserve(size);
            std::swap(requests, newRequests);
        }

        if (isResidentListDirty)
        {
            isResidentListDirty = false;

            const auto size = residents.size();
            residents.reserve(size);

            if (hasCancelledTask)
            {
                hasCancelledTask = false;

                auto pred = [](const auto& item) -> bool {
                    auto task = item.task;
                    if (unlikely(task == nullptr))
                    {
                        return true;
                    }

                    if (task->IsCancelled())
                    {
                        return true;
                    }

                    return false;
                };

                auto end = residents.end();
                end = std::remove_if(residents.begin(), end, pred);
                residents.resize(std::distance(residents.begin(), end));
            }

            const auto newItems = newResidents.size();
            residents.reserve(residents.size() + newItems);

            std::move(newResidents.begin(), newResidents.end(),
                std::back_inserter(residents));

            newResidents.clear();
        }

        flipCount.fetch_add(1, std::memory_order_relaxed);
    }

    void TaskStream::RunLoop(const std::atomic<bool>& isRunning)
    {
        if (unlikely(threadID != std::this_thread::get_id()))
        {
            auto log = Logger::Get(name);
            log.OutFatalError("Incorrect thread!");
            return;
        }

        const std::chrono::milliseconds waitPeriod(10);
        auto IsDirty = [this]() {
            return isDirty.load(std::memory_order_relaxed) ||
                residents.size() > 0;
        };

        while (likely(isRunning))
        {
            if (!IsDirty())
            {
                std::unique_lock lock(cvLock);
                cv.wait_for(lock, waitPeriod, IsDirty);
            }

            FlipBuffers();

            auto startTime = Time::GetNow();

            if (requests.size() > 0)
            {
                UpdateRequests();
            }

            if (residents.size() > 0)
            {
                UpdateResidents();
            }

            auto endTime = Time::GetNow();
            deltaTime = Time::ToFloat(endTime - startTime);

            if (unlikely(deltaTime > 0.1f))
            {
                auto log = Logger::Get(name);
                log.OutWarning([dt = deltaTime](auto& ls) {
                    ls << "Slow DeltaTime = " << dt;
                });
            }
        }
    }

    void TaskStream::UpdateRequests()
    {
        for (auto& request : requests)
        {
            auto task = request.task;
            if (unlikely(task == nullptr))
            {
                auto log = Logger::Get(name);
                log.OutError([name = name](auto& ls) {
                    ls << name.c_str() << " task func is null.";
                });

                continue;
            }

            task->Run(request.start, request.end);
        }

        requests.clear();
    }

    void TaskStream::UpdateResidents()
    {
        for (auto& request : residents)
        {
            auto task = request.task;
            if (unlikely(task == nullptr))
            {
                hasCancelledTask = true;

                auto log = Logger::Get(name);
                log.OutError([](auto& ls) { ls << "The task is null."; });

                continue;
            }

            if (unlikely(task->IsCancelled()))
            {
                auto log = Logger::Get(name);
                log.OutError([](auto& ls) { ls << "The task is cancelled."; });

                continue;
            }

            task->ForceRun();
        }
    }

} // namespace HE
