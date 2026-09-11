// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <algorithm>
#include "ComponentState.h"
#include "Debug.h"
#include "HSTL/HVector.h"
#include "String/String.h"

namespace hbe
{
/// @brief A template-based system for managing components through their lifecycle states (init, update, sleep, dead).
template<typename TComponent>
class ComponentSystem
{
	using TCompoList = hbe::HVector<TComponent>;

private:
	String name;

	TCompoList initList;
	TCompoList updateList;
	TCompoList swapUpdateList;
	TCompoList sleepList;

	TCompoList transitionList;

public:
	ComponentSystem(const char* name) :
		name(name), initList(), updateList(), swapUpdateList(), sleepList(), transitionList()
	{}

	[[nodiscard]] explicit operator bool() const noexcept { return !initList.empty() || !updateList.empty() || !sleepList.empty(); }

	[[nodiscard]] inline const char* getName() const noexcept { return name.toCharArray(); }

	template<typename... Types>
		TComponent& create(Types&&... args)
		{
			initList.emplace_back(std::forward<Types>(args)...);
			auto& compo = initList.back();
		compo.setState(ComponentState::BORN);

		return compo;
	}

	void update(const float deltaTime)
	{
		processInit();
		processUpdate(deltaTime);
		processTransition();
	}

private:
	void processInit()
	{
		for (auto& compo : initList)
		{
			compo.init();
			compo.setState(ComponentState::ALIVE);
			compo.onEnable();
			updateList.push_back(std::move(compo));
		}

		initList.clear();
	}

	void processUpdate(const float deltaTime)
	{
		for (auto& compo : updateList)
		{
			compo.update(deltaTime);

			if (!compo.isEnabled())
			{
				transitionList.push_back(std::move(compo));
			}
			else
			{
				swapUpdateList.push_back(std::move(compo));
			}
		}

		std::swap(updateList, swapUpdateList);
		swapUpdateList.clear();
	}

	void processTransition()
	{
		for (auto& compo : transitionList)
		{
			switch (compo.getState())
			{
				case ComponentState::ALIVE:
					compo.onEnable();
					updateList.push_back(std::move(compo));
					break;

				case ComponentState::SLEEP:
					compo.onDisable();
					sleepList.push_back(std::move(compo));
					break;

				case ComponentState::DEAD:
					compo.setState(ComponentState::SLEEP);
					compo.onDisable();
					compo.setState(ComponentState::DEAD);
					compo.release();
					break;

				default:
					Assert(false,
						   "Unexpected component state %d on processing "
						   "transition.\n",
						   compo.getState());
					break;
			}
		}

		transitionList.clear();
	}
};
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class ComponentSystemTest : public TestCollection
{
public:
	ComponentSystemTest() : TestCollection("ComponentSystemTest") {}

protected:
	void prepare() override;
};
} // namespace hbe
#endif //__UNIT_TEST__
