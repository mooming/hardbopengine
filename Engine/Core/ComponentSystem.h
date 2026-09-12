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

	[[nodiscard]] inline const char* GetName() const noexcept { return name.ToCharArray(); }

	template<typename... Types>
		TComponent& Create(Types&&... args)
		{
			initList.emplace_back(std::forward<Types>(args)...);
			auto& compo = initList.back();
		compo.SetState(ComponentState::BORN);

		return compo;
	}

	void Update(const float deltaTime)
	{
		ProcessInit();
		ProcessUpdate(deltaTime);
		ProcessTransition();
	}

private:
	void ProcessInit()
	{
		for (auto& compo : initList)
		{
			compo.Init();
			compo.SetState(ComponentState::ALIVE);
			compo.OnEnable();
			updateList.push_back(std::move(compo));
		}

		initList.clear();
	}

	void ProcessUpdate(const float deltaTime)
	{
		for (auto& compo : updateList)
		{
			compo.Update(deltaTime);

			if (!compo.IsEnabled())
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

	void ProcessTransition()
	{
		for (auto& compo : transitionList)
		{
			switch (compo.GetState())
			{
				case ComponentState::ALIVE:
					compo.OnEnable();
					updateList.push_back(std::move(compo));
					break;

				case ComponentState::SLEEP:
					compo.OnDisable();
					sleepList.push_back(std::move(compo));
					break;

				case ComponentState::DEAD:
					compo.SetState(ComponentState::SLEEP);
					compo.OnDisable();
					compo.SetState(ComponentState::DEAD);
					compo.Release();
					break;

				default:
					Assert(false,
						   "Unexpected component state %d on processing "
						   "transition.\n",
						   compo.GetState());
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
	void Prepare() override;
};
} // namespace hbe
#endif //__UNIT_TEST__
