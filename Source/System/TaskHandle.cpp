// Created by mooming.go@gmail.com, 2022

#include "TaskHandle.h"

#include "../Engine/Engine.h"
#include "Log/Logger.h"
#include "Task.h"

namespace HE
{

TaskHandle::TaskHandle()
    : key(InvalidKey),
      taskIndex(-1)
{
}

TaskHandle::TaskHandle(TKey key, TIndex taskIndex, TReleaser releaser)
    : key(key),
      taskIndex(taskIndex),
      releaser(std::move(releaser))
{
}

TaskHandle::TaskHandle(TaskHandle&& rhs)
    : key(rhs.key),
      taskIndex(rhs.taskIndex),
      releaser(std::move(rhs.releaser))
{
    rhs.key = InvalidKey;
    rhs.taskIndex = -1;
    rhs.releaser = nullptr;
}

TaskHandle::~TaskHandle()
{
    if (releaser == nullptr)
    {
        return;
    }

    if (!IsValid())
    {
        return;
    }

    releaser(*this);
}

TaskHandle& TaskHandle::operator=(TaskHandle&& rhs)
{
    this->~TaskHandle();

    key = rhs.key;
    taskIndex = rhs.taskIndex;
    releaser = std::move(rhs.releaser);

    rhs.key = InvalidKey;
    rhs.taskIndex = -1;
    rhs.releaser = nullptr;

    return *this;
}

bool TaskHandle::IsValid() const
{
    return key != InvalidKey && taskIndex >= 0;
}

Task* TaskHandle::GetTask() const
{
    auto& engine = Engine::Get();
    auto& taskSys = engine.GetTaskSystem();
    auto task = taskSys.GetTask(key, taskIndex);

    return task;
}

void TaskHandle::Wait(uint32_t intervalMilliSecs)
{
    if (!IsValid())
    {
        return;
    }

    auto task = GetTask();
    if (task == nullptr)
    {
        return;
    }

    if (task->IsDone())
    {
        return;
    }

    task->Wait(intervalMilliSecs);
}

void TaskHandle::BusyWait()
{
    if (!IsValid())
    {
        return;
    }

    auto task = GetTask();
    if (task == nullptr)
    {
        return;
    }

    if (task->IsDone())
    {
        return;
    }

    task->BusyWait();
}

void TaskHandle::Reset()
{
    this->~TaskHandle();
    new (this) TaskHandle();
}

} // namespace HE
