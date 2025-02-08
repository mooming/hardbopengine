// Created by mooming.go@gmail.com, 2022

#pragma once
#include "Container/Array.h"
#include <functional>

namespace HE
{

    class Task;

    class TaskHandle final
    {
    public:
        using TKey = uint32_t;
        using TIndex = Array<int>::TIndex;
        using TReleaser = std::function<void(const TaskHandle &handle)>;

        static constexpr TKey InvalidKey = 0;

    private:
        TKey key;
        TIndex taskIndex;
        TReleaser releaser;

    public:
        TaskHandle(const TaskHandle &rhs) = delete;
        TaskHandle &operator=(const TaskHandle &rhs) = delete;

    public:
        TaskHandle();
        TaskHandle(TKey key, TIndex taskIndex, TReleaser releaser);
        TaskHandle(TaskHandle &&rhs);
        ~TaskHandle();

        TaskHandle &operator=(TaskHandle &&rhs);

        bool IsValid() const;
        Task *GetTask() const;

        void Wait(uint32_t intervalMilliSecs);
        void BusyWait();
        void Reset();

        inline auto GetKey() const { return key; }
        inline auto GetIndex() const { return taskIndex; }
    };

} // namespace HE
