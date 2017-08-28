// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef TestCase_h
#define TestCase_h

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

#endif // TestCase_h