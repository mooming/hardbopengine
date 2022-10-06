// Created by mooming.go@gmail.com, 2022

#include "TaskSystem.h"

#include "HSTL/HString.h"
#include "Log/Logger.h"
#include "OSAL/OSThread.h"
#include "String/StringBuilder.h"
#include "String/StringUtil.h"
#include <algorithm>
#include <thread>


namespace HE
{

bool TaskSystem::TaskHandle::IsValid() const
{
    return key != InvalidKey && task.HasValue();
}

void TaskSystem::TaskHandle::Wait(uint32_t intervalMilliSecs)
{
    if (!IsValid())
        return;

    if ((*task).IsDone())
        return;

    (*task).Wait(intervalMilliSecs);
}

void TaskSystem::TaskHandle::BusyWait()
{
    if (!IsValid())
        return;

    auto& taskInstance = *task;
    if (taskInstance.IsDone())
        return;

    taskInstance.BusyWait();
}

TaskSystem::TaskSystem()
    : name("TaskSystem")
    , numHardwareThreads(std::thread::hardware_concurrency())
    , numWorkerThreads(numHardwareThreads - 1)
    , keySeed(1)
    , workerIndexStart(0)
    , numWorkers(0)
    , isRunning(false)
    , threadNames(numHardwareThreads)
    , threads(numHardwareThreads)
    , slots(numHardwareThreads)
{
    FatalAssert(numHardwareThreads > 0, "It should have at least one hardware thread.");
}

TaskSystem::~TaskSystem()
{
    Shutdown();
}

void TaskSystem::Initialize()
{
    auto log = Logger::Get(GetName());
    log.Out([this](auto& ls)
    {
        ls << "Hardware Concurrency = " << numHardwareThreads;
    });

    BuildStreams();
}

void TaskSystem::Shutdown()
{
    if (!isRunning)
        return;

    isRunning = false;

    for (auto& thread : threads)
    {
        if (unlikely(!thread.joinable()))
            continue;

        thread.join();
    }
}

void TaskSystem::PostUpdate()
{
    FlushDone();
}

StaticString TaskSystem::GetCurrentThreadName() const
{
    auto currentThreadId = std::this_thread::get_id();

    auto length = threads.Size();
    for (TIndex i = 0; i < length; ++i)
    {
        auto& thread = threads[i];
        if (thread.get_id() == currentThreadId)
        {
            return GetThreadName(i);
        }
    }

    static StaticString unknown("Unknown");
    return unknown;
}

StaticString TaskSystem::GetThreadName(int index) const
{
    if (unlikely(index <= 0 || index >= threadNames.Size()))
    {
        static StaticString unknown("Unknown");
        return unknown;
    }
    
    return threadNames[index];
}

TaskSystem::ThreadID TaskSystem::SetThread(TIndex index
   , StaticString name, std::function<void()> func)
{
    Assert(threadNames.Size() == threads.Size());

    if (unlikely(!threadNames.IsValidIndex(index)))
    {
        auto log = Logger::Get(GetName());
        log.OutError([this, index](auto& ls)
        {
            ls << "The given index(" << index << ") is not within the valid "
                << "range[0, " << threadNames.Size() << ")";
        });

        return std::thread::id();
    }

    threadNames[index] = name;

    auto& thread = threads[index];
    thread = Thread(func);

    Assert(thread.get_id() != std::thread::id());

    return thread.get_id();
}

TaskSystem::TIndex TaskSystem::GetCurrentThreadIndex() const
{
    return GetThreadIndex(std::this_thread::get_id());
}

TaskSystem::TIndex TaskSystem::GetThreadIndex(ThreadID id) const
{
    TIndex index = -1;
    auto size = threads.Size();
    for (TIndex i = 0; i < size; ++i)
    {
        auto& thread = threads[i];
        if (thread.get_id() != id)
            continue;

        index = i;
        break;
    }

    return index;
}

TaskSystem::TaskHandle TaskSystem::AllocateTask(StaticString name
    , TaskIndex size, Runnable func)
{
    TaskHandle taskHandle;
    if (unlikely(func == nullptr))
        return taskHandle;

    auto length = slots.Size();
    for (TIndex index = 0; index < length; ++index)
    {
        auto& slot = slots[index];
        auto& task = slot.task;
        if (task.HasValue())
            continue;

        auto key = IssueTaskKey();
        slot.key = key;

        task.Emplace(name, size, func);
        taskHandle.key = key;
        taskHandle.index = index;
        taskHandle.task.Emplace(task.Value());

        TIndex start = 0;
        TIndex i = workerIndexStart;

        if (size > 0)
        {
            const auto numWorkers = std::min<TIndex>(size, numHardwareThreads - workerIndexStart);
            Assert(numWorkers > 0);

            const TIndex interval = size / numWorkers;
            TIndex end = interval;

            const auto length = i + numWorkers - 1;
            for (; i < length; ++i)
            {
                streams[i].Request(*task, start, end);
                
                start = end;
                end += interval;
            }
        }

        streams[i].Request(*task, start, size);

        return taskHandle;
    }

    return taskHandle;
}

TaskSystem::TaskHandle TaskSystem::AllocateTask(StaticString name
    , TaskIndex size, Runnable func, TIndex numStreams)
{
    TaskHandle taskHandle;

    auto length = slots.Size();
    for (TIndex index = 0; index < length; ++index)
    {
        auto& slot = slots[index];
        if (slot.key == !InvalidKey)
            continue;

        auto key = IssueTaskKey();
        slot.key = key;

        auto& task = slot.task;
        task.Emplace(name, size, func);
        taskHandle.key = key;
        taskHandle.index = index;
        taskHandle.task.Emplace(task.Value());

        TIndex start = 0;
        TIndex i = workerIndexStart;

        if (size > 0)
        {
            const auto numWorkers = numHardwareThreads - workerIndexStart;
            Assert(numWorkers > 0);

            auto num = std::min<TIndex>(numWorkers, std::min<TIndex>(numStreams, size));
            const TIndex interval = size / num;
            TIndex end = interval;

            const auto length = i + num - 1;
            for (; i < length; ++i)
            {
                streams[i].Request(*task, start, end);

                start = end;
                end += interval;
            }
        }

        streams[i].Request(*task, start, size);

        return taskHandle;
    }

    return taskHandle;
}

void TaskSystem::DeallocateTask(TaskHandle&& handle)
{
    const auto key = handle.key;
    if (unlikely(key == InvalidKey))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([func = __PRETTY_FUNCTION__](auto& ls)
        {
            InlinePoolAllocator<char, 128> alloc;
            AllocatorScope scope(alloc);
            
            using namespace StringUtil;
            ls << '[' << PrettyFunctionToMethodName(func)
                << "] Failed due to the invalid key input.";
        });

        return;
    }

    const auto index = handle.index;
    if (unlikely(!slots.IsValidIndex(index)))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([func = __PRETTY_FUNCTION__, index](auto& ls)
        {
            InlinePoolAllocator<char, 128> alloc;
            AllocatorScope scope(alloc);

            using namespace StringUtil;
            ls << '[' << PrettyFunctionToMethodName(func)
                << "] Failed due to the invalid index " << index;
        });

        return;
    }

    auto& slot = slots[index];
    if (unlikely(key != slot.key))
    {
        auto log = Logger::Get(GetName());
        log.OutWarning([func = __PRETTY_FUNCTION__, key, &slot](auto& ls)
        {
            InlinePoolAllocator<char, 128> alloc;
            AllocatorScope scope(alloc);

            using namespace StringUtil;
            ls << '[' << PrettyFunctionToMethodName(func)
                << "] Key Mismatched: " << key << " <=> " << slot.key ;
        });

        return;
    }

    FatalAssert(&(*(handle.task)) == &(*(slot.task)));

    slot.key = InvalidKey;
    slot.task.Reset();

    handle.key = InvalidKey;
    handle.index = -1;
    handle.task.Reset();
}

void TaskSystem::FlushDone()
{
    for (auto& slot : slots)
    {
        auto& task = slot.task;
        if (!task.HasValue())
            continue;

        if (!(*task).IsDone())
            continue;

        slot.key = InvalidKey;
        task.Reset();
    }
}

void TaskSystem::BuildStreams()
{
    FatalAssert(numHardwareThreads >= 4, "System minimum requirement: quad cores");

    workerIndexStart = 0;

    auto log = Logger::Get(GetName());
    log.Out("# Creating TaskStreams ======================");

    streams.Swap(Array<TaskStream>(numHardwareThreads));
    streams.Emplace(GetMainTaskStreamIndex(), 0, "MainTaskStream");
    streams.Emplace(GetIOTaskStreamIndex(), 0, "IOTaskStream");

    auto& mainThread = GetMainTaskStream();
    auto& ioThread = GetIOTaskStream();

    mainThread.InitiateFromCurrentThread();
    ioThread.Start();

    workerIndexStart = GetIOTaskStreamIndex() + 1;

    InlineStringBuilder<64> streamName;
    for (TIndex i = workerIndexStart; i < numHardwareThreads; ++i)
    {
        ++numWorkers;
        streamName << "WorkStream[" << numWorkers << ']';
        
        streams.Emplace(i, i, streamName.c_str());
        streamName.Clear();
    }

    log.Out("# Starting TaskStreams ======================");

    isRunning = true;
    for (TIndex i = workerIndexStart; i < numHardwareThreads; ++i)
    {
        streams[i].Start();
    }
}

void TaskSystem::SetCPUAffinities()
{
    auto& mainTaskStream = GetMainTaskStream();
    auto& mainThread = threads[mainTaskStream.GetThreadIndex()];
    OS::SetThreadAffinity(mainThread, 1);

    auto& ioTaskStream = GetIOTaskStream();
    auto& ioThread = threads[ioTaskStream.GetThreadIndex()];
    OS::SetThreadAffinity(ioThread, 8);
}

TaskSystem::TKey TaskSystem::IssueTaskKey()
{
    for (auto& slot : slots)
    {
        if (unlikely(keySeed == 0))
            ++keySeed;

        if (likely(slot.key != keySeed))
            break;

        ++keySeed;
    }

    Assert(keySeed != InvalidKey);
    return keySeed++;
}

} // HE

#ifdef __UNIT_TEST__
#include "Engine.h"
#include "Test/TestCollection.h"


namespace HE
{

void TaskSystemTest::Prepare()
{
    AddTest("Empty Task", [this](auto& ls)
    {
        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        auto handle = taskSys.AllocateTask("Test", 0, nullptr);
        if (handle.IsValid())
        {
            ls << "Handle should be invalid if func is null." << lferr;
        }

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });

    AddTest("Task of size 0", [this](auto& ls)
    {
        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();

        auto func = [](auto start, auto end)
        {
            auto log = Logger::Get("Size 0 Task");
            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << ')';
            });
        };

        auto handle = taskSys.AllocateTask("Test", 0, func);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
        }

        handle.BusyWait();

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });

    AddTest("Sum 1 to 10000", [this](auto& ls)
    {
        std::atomic<uint64_t> sum = 0;

        auto func = [&sum](auto start, auto end)
        {
            uint64_t result = 0;
            for (uint64_t i = start + 1; i <= end; ++i)
            {
                result += i;
            }

            sum += result;

            auto log = Logger::Get("Sum 1 to 10000");
            log.Out([&](auto& ls)
            {
                ls << "Sum Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
        };

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        auto handle = taskSys.AllocateTask("Test", 10000000, func);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
        }

        handle.BusyWait();
        auto& task = *(handle.task);
        ls << "Sum = " << sum << ", done = " << task.NumDone()
            << "/" << task.NumStreams() << lf;

        if (sum != 50000005000000L)
        {
            ls << "Incorrect summation result = " << sum << lferr;
        }

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });

    AddTest("Number of Prime Numbers", [this](auto& ls)
    {
        auto IsPrimeNumbrer = [](uint32_t value) -> bool
        {
            for (uint32_t i = 2; i < value; ++i)
            {
                if ((value % i) == 0)
                    return false;
            }

            return true;
        };

        std::atomic<uint64_t> count = 0;

        auto func = [&count, IsPrimeNumbrer](auto start, auto end)
        {
            auto log = Logger::Get("Count Prime Numbers");
            log.Out([&](auto& ls)
            {
                ls << "Start: Range[" << (start + 1) << ", " << end << ']';
            });

            uint64_t result = 0;

            for (uint64_t i = start + 1; i <= end; ++i)
            {
                if (IsPrimeNumbrer(i))
                    ++result;
            }

            count += result;

            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
        };

        constexpr int upperBound = 1000000;
        constexpr int solution = 78498;

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        auto handle = taskSys.AllocateTask("Count Prime Numbers", upperBound, func);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
        }

        handle.BusyWait();
        count = count - 1;

        auto& task = *(handle.task);
        ls << "# of Prime Numbers = " << count << ", done = " << task.NumDone()
            << "/" << task.NumStreams() << lf;

        if (count != solution)
        {
            ls << "Number of prime numbers below " << upperBound
                << " should be " << solution <<" but "
                << count << " found!" << lferr;
        }

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });

    AddTest("1 Thread", [this](auto& ls)
    {
        auto IsPrimeNumbrer = [](uint32_t value) -> bool
        {
            for (uint32_t i = 2; i < value; ++i)
            {
                if ((value % i) == 0)
                    return false;
            }

            return true;
        };

        std::atomic<uint64_t> count = 0;
        std::atomic<int> numThreads = 0;

        auto func = [&count, &numThreads, IsPrimeNumbrer](auto start, auto end)
        {
            auto log = Logger::Get("Count Prime Numbers");
            log.Out([&](auto& ls)
            {
                ls << "Start: Range[" << (start + 1) << ", " << end << ']';
            });

            uint64_t result = 0;

            for (uint64_t i = start + 1; i <= end; ++i)
            {
                if (IsPrimeNumbrer(i))
                    ++result;
            }

            count += result;

            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
            ++numThreads;
        };

        constexpr int upperBound = 1000;
        constexpr int solution = 168;

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        auto handle = taskSys.AllocateTask("Count Prime Numbers", upperBound, func, 1);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
            return;
        }

        handle.BusyWait();
        count = count - 1;

        if (numThreads != 1)
        {
            ls << "Number of threads must be 1, but " << numThreads << lferr;
        }

        auto& task = *(handle.task);
        ls << "# of Prime Numbers = " << count << ", done = " << task.NumDone()
            << "/" << task.NumStreams() << lf;

        if (count != solution)
        {
            ls << "Number of prime numbers below " << upperBound
                << " should be " << solution << " but "
                << count << " found!" << lferr;
        }

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });

    AddTest("2 Threads", [this](auto& ls)
    {
        auto IsPrimeNumbrer = [](uint32_t value) -> bool
        {
            for (uint32_t i = 2; i < value; ++i)
            {
                if ((value % i) == 0)
                    return false;
            }

            return true;
        };

        std::atomic<uint64_t> count = 0;
        std::atomic<int> numThreads = 0;

        auto func = [&count, &numThreads, IsPrimeNumbrer](auto start, auto end)
        {
            ++numThreads;

            auto log = Logger::Get("Count Prime Numbers");
            log.Out([&](auto& ls)
            {
                ls << "Start: Range[" << (start + 1) << ", " << end << ']';
            });

            uint64_t result = 0;

            for (uint64_t i = start + 1; i <= end; ++i)
            {
                if (IsPrimeNumbrer(i))
                    ++result;
            }

            count += result;

            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
        };

        constexpr int upperBound = 1000;
        constexpr int solution = 168;

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        auto handle = taskSys.AllocateTask("Count Prime Numbers", upperBound, func, 2);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
            return;
        }

        handle.BusyWait();
        count = count - 1;

        if (numThreads != 2)
        {
            ls << "Number of threads must be 1, but " << numThreads << lferr;
        }

        auto& task = *(handle.task);
        ls << "# of Prime Numbers = " << count << ", done = " << task.NumDone()
            << "/" << task.NumStreams() << lf;

        if (count != solution)
        {
            ls << "Number of prime numbers below " << upperBound
                << " should be " << solution <<" but "
                << count << " found!" << lferr;
        }

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });

    AddTest("3 Threads", [this](auto& ls)
    {
        auto IsPrimeNumbrer = [](uint32_t value) -> bool
        {
            for (uint32_t i = 2; i < value; ++i)
            {
                if ((value % i) == 0)
                    return false;
            }

            return true;
        };

        std::atomic<uint64_t> count = 0;
        std::atomic<int> numThreads = 0;

        auto func = [&count, &numThreads, IsPrimeNumbrer](auto start, auto end)
        {
            ++numThreads;

            auto log = Logger::Get("Count Prime Numbers");
            log.Out([&](auto& ls)
            {
                ls << "Start: Range[" << (start + 1) << ", " << end << ']';
            });

            uint64_t result = 0;

            for (uint64_t i = start + 1; i <= end; ++i)
            {
                if (IsPrimeNumbrer(i))
                    ++result;
            }

            count += result;

            log.Out([&](auto& ls)
            {
                ls << "Range[" << (start + 1)
                    << ", " << end << "] = " << result;
            });
        };

        constexpr int upperBound = 1000;
        constexpr int solution = 168;

        auto& engine = Engine::Get();
        auto& taskSys = engine.GetTaskSystem();
        auto handle = taskSys.AllocateTask("Count Prime Numbers", upperBound, func, 3);
        if (!handle.IsValid())
        {
            ls << "Handle should not be invalid." << lferr;
            return;
        }

        handle.BusyWait();
        count = count - 1;

        if (numThreads != 3)
        {
            ls << "Number of threads must be 1, but " << numThreads << lferr;
        }

        auto& task = *(handle.task);
        ls << "# of Prime Numbers = " << count << ", done = " << task.NumDone()
            << "/" << task.NumStreams() << lf;

        if (count != solution)
        {
            ls << "Number of prime numbers below " << upperBound
                << " should be " << solution << " but "
                << count << " found!" << lferr;
        }

        taskSys.DeallocateTask(std::move(handle));
        taskSys.FlushDone();
    });
}

} // HE
#endif //__UNIT_TEST__
