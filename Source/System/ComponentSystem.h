// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef __ComponentSystem_h__
#define __ComponentSystem_h__

#include "ComponentState.h"
#include "Debug.h"
#include "Vector.h"
#include "String.h"

#include <algorithm>

namespace HE
{
    template <typename Component>
    class ComponentSystem
    {
        using CompoList = Vector<Component>;

    private:
        String name;

        CompoList initList;
        CompoList updateList;
        CompoList swapUpdateList;
        CompoList sleepList;

        CompoList transitionList;

    public:
        inline ComponentSystem(const char* name) : name(name)
            , initList()
            , updateList()
            , swapUpdateList()
            , sleepList()
            , transitionList()
        {
        }

        inline operator bool() const
        {
            return !initList.empty()
                || !updateList.empty()
                || !sleepList.empty();
        }

        inline const char* GetName() const { return name.ToCharArray(); }

        template <typename ... Types>
        inline Component& Create(Types&& ... args)
        {
			initList.emplace_back(std::forward<Types>(args) ...);
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
        inline void ProcessInit()
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

        inline void ProcessUpdate(const float deltaTime)
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

        inline void ProcessTransition()
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
                    AssertMessage(false
                        , "Unexpected component state %d on processing transition.\n"
                        , compo.GetState());
                    break;
                }
            }

            transitionList.clear();
        }
    };
}

#ifdef __UNIT_TEST__
#include "TestCase.h"

namespace HE
{
    class ComponentSystemTest : public TestCase
    {
    public:
        ComponentSystemTest() : TestCase("ComponentSystemTest") {}

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__

#endif //__ComponentSystem_h__
