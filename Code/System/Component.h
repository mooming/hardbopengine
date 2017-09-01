// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef __Component_h__
#define __Component_h__

#include "System/String.h"

namespace HE
{
    class Component
    {

    public:
        enum class State
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
        virtual void PostUpdate(const float deltaTime) = 0;
        virtual void Release() = 0;

        virtual void OnEnable() = 0;
        virtual void OnDisable() = 0;

    public:
        inline Component(const char* name) : state(NONE), name(name) {}
        virtual ~Component() = default;

        inline State GetState() const { return state; }
        inline void SetState(State state) { Component::state = state; }

        inline const String& GetName() const { return name; }
        inline bool IsEnabled() const { return state == ALIVE; }

        inline void SetEnable(bool isEnabled)
        {
            SetState(isEnabled ? ALIVE : SLEEP);
        }
        
        inline void Destroy()
        {
            SetState(State.DEAD);
        }
    };
}

#endif //__Component_h__
