// Created by mooming.go@gmail.com, 2017

#pragma once

#include "ComponentState.h"
#include "String/String.h"

namespace HE
{
    class Component
    {
        using State = ComponentState;

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
        inline Component(const char* name)
            : state(State::NONE),
              name(name)
        {
        }

        virtual ~Component() = default;

        inline State GetState() const { return state; }

        inline void SetState(State state) { Component::state = state; }

        inline const String& GetName() const { return name; }

        inline bool IsEnabled() const { return state == State::ALIVE; }

        inline void SetEnable(bool isEnabled)
        {
            SetState(isEnabled ? State::ALIVE : State::SLEEP);
        }

        inline void Destroy() { SetState(State::DEAD); }
    };
} // namespace HE
