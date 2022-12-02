// Created by mooming.go@gmail.com, 2022

#pragma once

#include "Buffer.h"
#include "Resource.h"
#include "HSTL/HVector.h"
#include "String/StaticString.h"


namespace HE
{

class Engine;
class TaskSystem;

class ResourceManager final
{
    template <typename T>
    using TVector = HSTL::HVector<T>;

private:
    struct ResourceItem final
    {
        uint32_t id = 0;
        uint32_t referenceCount = 0;

        StaticString path;
        Buffer buffer;
    };

    struct LoadingRequest final
    {
        uint32_t resourceID = 0;
        StaticString path;
    };

    TVector<ResourceItem> resources;
    TVector<ResourceItem*> loadingRequests;

public:
    ResourceManager();
    ~ResourceManager();

    void PostUpdate(Engine& engine);

    Resource RequestLoad(StaticString path);
    Resource Load(StaticString path);

private:
    void RequestTasks(TaskSystem& taskSys);
};

} // HE
