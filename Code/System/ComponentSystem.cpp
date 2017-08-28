// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "System/ComponentSystem.h"

#ifdef __UNIT_TEST__
using namespace HE;

#include "Component.h"

#include "Time.h"

#include <iostream>

bool ComponentSystemTest::DoTest()
{

#ifdef __DEBUG__
    constexpr int componentNum = 1024;
    constexpr int updateNum = 60;
#else //__DEBUG__
    constexpr int componentNum = 4096 * 5;
    constexpr int updateNum = 600;
#endif //__DEBUG__

    using namespace std;

    class Test : public Component
    {
    public:
        bool isInit = false;
        uint32_t updateCount = 0;
        bool isRelased = false;
        bool isOnEnableCalled = false;
        bool isOnDisableCalled = false;

        bool& testResult;

        Test(bool& testResult) : Component("Test"), testResult(testResult)
        {
        }

        Test(Test&& rhs) : Component(std::move(rhs))
            , isInit(rhs.isInit)
            , updateCount(rhs.updateCount)
            , isRelased(rhs.isRelased)
            , isOnEnableCalled(rhs.isOnEnableCalled)
            , isOnDisableCalled(rhs.isOnDisableCalled)
            , testResult(rhs.testResult)
        {
        }

        Test& operator= (const Test& rhs)
        {
            static_cast<Component&>(*this) = static_cast<const Component&>(rhs);

            isInit = rhs.isInit;
            updateCount = rhs.updateCount;
            isRelased = rhs.isRelased;
            isOnEnableCalled = rhs.isOnEnableCalled;
            isOnDisableCalled = rhs.isOnDisableCalled;
            testResult = rhs.testResult;

            return *this;
        }

        virtual ~Test()
        {
            if (!isInit || !isOnEnableCalled || !isOnDisableCalled || !isRelased)
            {
                testResult = false;
            }
        }

        virtual void Init() override
        {
            if (GetState() != BORN)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << BORN << endl;
                testResult = false;
            }

            isInit = true;
        }

        virtual void Update(const float deltaTime) override
        {
            if (GetState() != ALIVE)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ALIVE << endl;
                testResult = false;
            }

            ++updateCount;

            if (updateCount > updateNum)
            {
                //cout << "Updated " << updateCount << " times." << endl;
                Destroy();
            }
        }

        virtual void Release() override
        {
            if (GetState() != DEAD)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << DEAD << endl;
                testResult = false;
            }

            isRelased = true;
        }

        virtual void OnEnable() override
        {
            if (GetState() != ALIVE)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ALIVE << endl;
                testResult = false;
            }

            isOnEnableCalled = true;
        }

        virtual void OnDisable() override
        {
            if (GetState() != SLEEP)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << SLEEP << endl;
                testResult = false;
            }

            isOnDisableCalled = true;
        }
    };

    ComponentSystem<Test> testSystem("TestSystem");
    bool testResult = true;

    for (int i = 0; i < componentNum; ++i)
    {
        testSystem.Create(testResult);
    }

    float loopTime = 0.0f;
    {
        Time::MeasureSec measure(loopTime);
        while (testSystem)
        {
            testSystem.Update(0.033f);
            if (!testResult)
            {
                return false;
            }
        }
    }

    cout << "Total Loop Time = " << loopTime << endl;

    return true;
}
#endif //__UNIT_TEST__
