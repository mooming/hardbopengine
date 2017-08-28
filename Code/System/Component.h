// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef __Component_h__
#define __Component_h__

#include "System/String.h"

namespace HE
{
    class Component
    {
        // ==================================================================================
        // Life Cycle
        // 0. Born
        // 1. Init
        // 2. OnEnable
        // 3. Update
        // 4. OnDisable
        // 5. OnEnable
        // 6. Update
        // 7. OnDisable
        // 8. Release
        // 9. Death
        //
        // State
        // 1. Born
        // 2. Alive
        // 3. Sleep
        // 4. Dead

    public:
        enum State
        {
            NONE,
            BORN,
            ALIVE,
            SLEEP,
            DEAD
        };

    protected:
        State state;
        String name;

    public:
        virtual void Init() = 0;
        virtual void Update(const float deltaTime) = 0;
        virtual void Release() = 0;

        virtual void OnEnable() = 0;
        virtual void OnDisable() = 0;

    public:
        inline Component(const char* name) : state(NONE), name(name) {}
        inline Component(const Component& rhs) : state(rhs.state), name(rhs.name) {}
        inline Component(Component&& rhs) : state(rhs.state), name(std::move(rhs.name)) {}
        virtual ~Component() = default;

        Component& operator= (const Component& rhs)
        {
            state = rhs.state;
            name = rhs.name;
            return *this;
        }

        Component& operator= (Component&& rhs)
        {
            state = rhs.state;
            name = std::move(rhs.name);

            return *this;
        }

        inline State GetState() const { return state; }
        inline void SetState(State state) { Component::state = state; }

        inline const char* GetName() const { return name.ToCharArray(); }
        inline bool IsEnabled() const { return state == ALIVE; }

        inline void SetEnable(bool isEnabled) { state = isEnabled ? ALIVE : SLEEP; }
        inline void Destroy() { state = DEAD; }
    };
}

#endif //__Component_h__
