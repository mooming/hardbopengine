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
	virtual void init() = 0;
	virtual void update(const float deltaTime) = 0;
	virtual void release() = 0;

	virtual void onEnable() = 0;
	virtual void onDisable() = 0;

public:
	Component(const char* name) : state(State::NONE), name(name) {}
	virtual ~Component() = default;
	[[nodiscard]] State getState() const noexcept { return state; }
	void setState(State inState) noexcept { Component::state = inState; }
	[[nodiscard]] const String& getName() const noexcept { return name; }
	[[nodiscard]] bool isEnabled() const noexcept { return state == State::ALIVE; }
	void setEnable(bool isEnabled) noexcept { setState(isEnabled ? State::ALIVE : State::SLEEP); }
	void destroy() noexcept { setState(State::DEAD); }
};
} // namespace hbe
