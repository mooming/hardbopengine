// Created by mooming.go@gmail.com, 2017

#include "System/ComponentSystem.h"

#ifdef __UNIT_TEST__
using namespace HE;

#include "Component.h"
#include "ComponentState.h"

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
        bool isReleased = false;
        bool isOnEnableCalled = false;
        bool isOnDisableCalled = false;
        bool isValid = true;
        bool& testResult;

    public:
        Test(bool& testResult) : Component("Test"), testResult(testResult)
        {
        }

        Test(const Test&) = delete;

        Test(Test&& rhs) noexcept : Component(rhs)
            , isInit(rhs.isInit)
            , updateCount(rhs.updateCount)
            , isReleased(rhs.isReleased)
            , isOnEnableCalled(rhs.isOnEnableCalled)
            , isOnDisableCalled(rhs.isOnDisableCalled)
            , testResult(rhs.testResult)
        {
            rhs.isValid = false;
        }

        Test& operator= (const Test& rhs)
        {
            static_cast<Component&>(*this) = static_cast<const Component&>(rhs);

            isInit = rhs.isInit;
            updateCount = rhs.updateCount;
            isReleased = rhs.isReleased;
            isOnEnableCalled = rhs.isOnEnableCalled;
            isOnDisableCalled = rhs.isOnDisableCalled;
            testResult = rhs.testResult;

            return *this;
        }

        virtual ~Test()
        {
            if (!isValid)
                return;
            
            if (!isInit || !isOnEnableCalled || !isOnDisableCalled || !isReleased)
            {
                testResult = false;
                cerr << "Unexpected Test Destoy" << endl;
            }
        }

        virtual void Init() override
        {
            if (GetState() != ComponentState::BORN)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ComponentState::BORN << endl;
                testResult = false;
            }

            isInit = true;
        }

        virtual void Update(const float deltaTime) override
        {
            if (GetState() != ComponentState::ALIVE)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ComponentState::ALIVE << endl;
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
            if (GetState() != ComponentState::DEAD)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ComponentState::DEAD << endl;
                testResult = false;
            }

            isReleased = true;
        }

        virtual void OnEnable() override
        {
            if (GetState() != ComponentState::ALIVE)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ComponentState::ALIVE << endl;
                testResult = false;
            }

            isOnEnableCalled = true;
        }

        virtual void OnDisable() override
        {
            if (GetState() != ComponentState::SLEEP)
            {
                cerr << "State failure, state = " << GetState()
                    << ", but expected " << ComponentState::SLEEP << endl;
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

    Time::TDuration loopTime;
    
    {
        Time::Measure measure(loopTime);
        while (testSystem)
        {
            testSystem.Update(0.033f);
            if (!testResult)
            {
                return false;
            }
        }
    }

    cout << "Total Loop Time = " << Time::ToMilliSec<float>(loopTime) << endl;

    return true;
}
#endif //__UNIT_TEST__
