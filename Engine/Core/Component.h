// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "ComponentState.h"
#include "String/String.h"

namespace hbe
{
/// @brief Base class for game engine components with lifecycle management (init, update, enable, disable, release).
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
	Component(const char* name) : state(State::NONE), name(name) {}
	virtual ~Component() = default;
	[[nodiscard]] State GetState() const noexcept { return state; }
	void SetState(State inState) noexcept { Component::state = inState; }
	[[nodiscard]] const String& GetName() const noexcept { return name; }
	[[nodiscard]] bool IsEnabled() const noexcept { return state == State::ALIVE; }
	void SetEnable(bool isEnabled) noexcept { SetState(isEnabled ? State::ALIVE : State::SLEEP); }
	void Destroy() noexcept { SetState(State::DEAD); }
};
} // namespace hbe
