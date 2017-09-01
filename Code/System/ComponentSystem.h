// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef __ComponentSystem_h__
#define __ComponentSystem_h__

#include "ComponentState.h"
#include "Debug.h"
#include "Vector.h"
#include "String.h"

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
            return !initList.IsEmpty()
                || !updateList.IsEmpty()
                || !sleepList.IsEmpty();
        }

        inline const char* GetName() const { return name.ToCharArray(); }

        template <typename ... Types>
        inline Component& Create(Types&& ... args)
        {
            auto& compo = initList.New(std::forward<Types>(args) ...);
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
                updateList.Add(std::move(compo));
            }

            initList.ChangeSize(0);
        }

        inline void ProcessUpdate(const float deltaTime)
        {
            for (auto& compo : updateList)
            {
                compo.Update(deltaTime);

                if (!compo.IsEnabled())
                {
                    transitionList.Add(std::move(compo));
                }
                else
                {
                    swapUpdateList.Add(std::move(compo));
                }
            }

            updateList.Swap(std::move(swapUpdateList));
            swapUpdateList.ChangeSize(0);
        }

        inline void ProcessTransition()
        {
            for (auto& compo : transitionList)
            {
                switch (compo.GetState())
                {
                case ComponentState::ALIVE:
                    compo.OnEnable();
                    updateList.Add(std::move(compo));
                    break;

                case ComponentState::SLEEP:
                    compo.OnDisable();
                    sleepList.Add(std::move(compo));
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

            transitionList.Clear();
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
