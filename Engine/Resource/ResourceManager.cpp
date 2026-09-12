// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ResourceManager.h"

#include "../Engine/Engine.h"
#include "Core/TaskSystem.h"


namespace hbe
{

	ResourceManager::ResourceManager() noexcept = default;

	ResourceManager::~ResourceManager() noexcept = default;

	void ResourceManager::PostUpdate(Engine& engine) noexcept {}

	void ResourceManager::RequestTasks(TaskSystem& taskSys) noexcept {}

} // namespace hbe
