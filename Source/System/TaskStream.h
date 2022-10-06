// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Task.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"
#include <functional>
#include <mutex>
#include <thread>


namespace HE
{

class TaskStream final
{
    template <typename T>
    using TVector = HSTL::HVector<T>;
    using TIndex = Task::TIndex;
    using ThreadID = std::thread::id;

    struct Request final
    {
        Task* task;
        TIndex start;
        TIndex end;

        Request();
        Request(Task& task, TIndex start, TIndex end);
        ~Request() = default;
    };

    using TRequests = TVector<Request>;

private:
    StaticString name;
    int threadIndex;
    ThreadID threadID;

    std::mutex queueLock;
    TRequests requests;
    TRequests buffer;

public:
    TaskStream();
    TaskStream(int threadIndex, StaticString name);
    ~TaskStream() = default;

    void InitiateFromCurrentThread();
    void Start();
    void Request(Task& task, TIndex start, TIndex end);
    void Flush();

    inline auto GetThreadIndex() const { return threadIndex; }
};

} // HE
