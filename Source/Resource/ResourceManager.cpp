// Created by mooming.go@gmail.com, 2022

#include "ResourceManager.h"

#include "Engine.h"
#include "System/TaskSystem.h"

namespace HE
{

    ResourceManager::ResourceManager()
    {
    }

    ResourceManager::~ResourceManager()
    {
    }

    void ResourceManager::PostUpdate(Engine &engine)
    {
    }

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

    void ResourceManager::RequestTasks(TaskSystem &taskSys)
    {
    }

} // namespace HE
