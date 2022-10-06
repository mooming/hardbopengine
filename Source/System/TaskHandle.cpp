// Created by mooming.go@gmail.com, 2022

#include "TaskHandle.h"

#include "Engine.h"
#include "Task.h"
#include "Log/Logger.h"


namespace HE
{

TaskHandle::TaskHandle()
    : key(InvalidKey)
    , taskIndex(-1)
{
}

TaskHandle::TaskHandle(TKey key, TIndex taskIndex)
    : key(key)
    , taskIndex(taskIndex)
{
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
        return;

    auto task = GetTask();
    if (task == nullptr)
        return;

    if (task->IsDone())
        return;

    task->Wait(intervalMilliSecs);
}

void TaskHandle::BusyWait()
{
    if (!IsValid())
        return;

    auto task = GetTask();
    if (task == nullptr)
        return;

    if (task->IsDone())
        return;

    task->BusyWait();
}

void TaskHandle::Reset()
{
    this->~TaskHandle();
    new (this) TaskHandle();
}

} // HE
