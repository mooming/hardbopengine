// Created by mooming.go@gmail.com, 2017

#pragma once

namespace HE
{
    class TestCase
    {
    private:
        const char* title;
        bool isDone;
        bool isSuccess;

    public:
        TestCase(const char* title);
        virtual ~TestCase() = default;

        const char* GetName() const { return title; }
        void Start();
        inline bool IsDone() const { return isDone; }
        inline bool IsSuccess() const { return isSuccess; }

    private:
        virtual void Report();

    protected:
        virtual bool DoTest() = 0;
    };
}
