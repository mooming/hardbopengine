// Created by mooming.go@gmail.com, 2022

#pragma once
#include "Container/Array.h"


namespace HE
{

class Task;

class TaskHandle final
{
public:
    using TKey = uint32_t;
    using TIndex = Array<int>::TIndex;
    static constexpr TKey InvalidKey = 0;
    
private:
    TKey key;
    TIndex taskIndex;

public:
    TaskHandle();
    TaskHandle(TKey key, TIndex taskIndex);
    ~TaskHandle() = default;

    bool IsValid() const;
    Task* GetTask() const;

    void Wait(uint32_t intervalMilliSecs);
    void BusyWait();
    void Reset();

    inline auto GetKey() const { return key; }
    inline auto GetIndex() const { return taskIndex; }
};

} // HE
