// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ResourceManager.h"

#include "../Engine/Engine.h"
#include "Core/TaskSystem.h"

namespace hbe
{

	ResourceManager::ResourceManager() {}

	ResourceManager::~ResourceManager() {}

	void ResourceManager::PostUpdate(Engine& engine) {}

	Resource ResourceManager::RequestLoad(StaticString path)
	{
		Assert(TaskSystem::IsMainThread());

		Resource resource;

		return resource;
	}

	Resource ResourceManager::Load(StaticString path)
	{
		Assert(TaskSystem::IsMainThread());

		Resource resource;

		return resource;
	}

	void ResourceManager::RequestTasks(TaskSystem& taskSys) {}

} // namespace hbe
