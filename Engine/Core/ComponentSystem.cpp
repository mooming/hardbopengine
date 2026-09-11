// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Core/ComponentSystem.h"

#ifdef __UNIT_TEST__
#include "Component.h"
#include "ComponentState.h"
#include "ScopedTime.h"


namespace hbe
{

void ComponentSystemTest::prepare()
{
	addTest("Update Component Test", [this](auto& ls)
	{
#ifdef __DEBUG__
		constexpr int componentNum = 1024;
		constexpr int updateNum = 60;
#else // !__DEBUG__
		constexpr int componentNum = 4096 * 5;
		constexpr int updateNum = 600;
#endif // __DEBUG__

		struct Test : public Component
		{
			bool isInit = false;
			uint32_t updateCount = 0;
			bool isReleased = false;
			bool isOnEnableCalled = false;
			bool isOnDisableCalled = false;
			bool isValid = true;
			bool& testResult;

			TestCollection::TLogOut& ls;
			TestCollection::LogFlush& lferr;

			Test(const Test&) = delete;

			Test(bool& testResult, TestCollection::TLogOut& ls, TestCollection::LogFlush& lferr) :
				Component("Test"), testResult(testResult), ls(ls), lferr(lferr)
			{}

			Test(Test&& rhs) noexcept :
				Component(rhs), isInit(rhs.isInit), updateCount(rhs.updateCount), isReleased(rhs.isReleased),
				isOnEnableCalled(rhs.isOnEnableCalled), isOnDisableCalled(rhs.isOnDisableCalled),
				testResult(rhs.testResult), ls(rhs.ls), lferr(rhs.lferr)
			{
				rhs.isValid = false;
			}

			Test& operator=(const Test& rhs)
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

					~Test() override
			{
				if (!isValid)
					return;

				if (!isInit || !isOnEnableCalled || !isOnDisableCalled || !isReleased)
				{
					testResult = false;
					ls << "Test object is destroyed accidently." << lferr;
				}
			}

			void init() override
			{
				if (getState() != ComponentState::BORN)
				{
					ls << "State failure, state = " << getState() << ", but expected " << ComponentState::BORN
					   << lferr;

					testResult = false;
				}

				isInit = true;
			}

			void update(const float deltaTime) override
			{
				if (getState() != ComponentState::ALIVE)
				{
					ls << "State failure, state = " << getState() << ", but expected " << ComponentState::ALIVE
					   << lferr;
					testResult = false;
				}

				++updateCount;

				if (updateCount > updateNum)
				{
					destroy();
				}
			}

			void release() override
			{
				if (getState() != ComponentState::DEAD)
				{
					ls << "State failure, state = " << getState() << ", but expected " << ComponentState::DEAD
					   << lferr;

					testResult = false;
				}

				isReleased = true;
			}

			void onEnable() override
			{
				if (getState() != ComponentState::ALIVE)
				{
					ls << "State failure, state = " << getState() << ", but expected " << ComponentState::ALIVE
					   << lferr;

					testResult = false;
				}

				isOnEnableCalled = true;
			}

			void onDisable() override
			{
				if (getState() != ComponentState::SLEEP)
				{
					ls << "State failure, state = " << getState() << ", but expected " << ComponentState::SLEEP
					   << lferr;

					testResult = false;
				}

				isOnDisableCalled = true;
			}
		};

		ComponentSystem<Test> testSystem("TestSystem");

		bool testResult = true;
		for (int i = 0; i < componentNum; ++i)
		{
			testSystem.create(testResult, ls, lferr);
		}

		time::TDuration loopTime;

		{
			time::ScopedTime measure(loopTime);
			while (testSystem)
			{
				testSystem.update(0.033f);

				if (!testResult)
					return;
			}
		}

		ls << "Total Loop Time = " << time::toFloat(loopTime) << lf;
	});
}

} // namespace hbe

#endif //__UNIT_TEST__
