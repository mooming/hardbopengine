// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Task.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"
#include <mutex>
#include <thread>


namespace HE
{

class TaskStream final
{
    template <typename T>
    using TVector = HSTL::HVector<T>;
    using TIndex = Task::TIndex;

    struct Request final
    {
        Task* task;
        TIndex start;
        TIndex end;

        Request();
        Request(Task& task, TIndex start, TIndex end);
        ~Request() = default;
    };

private:
    StaticString name;
    int threadIndex;

    std::mutex queueLock;
    TVector<Request> requests;
    TVector<Request> buffer;

public:
    TaskStream();
    TaskStream(int threadIndex, StaticString name);
    ~TaskStream() = default;

    void Start();
    void Request(Task& task, TIndex start, TIndex end);
};

} // HE
