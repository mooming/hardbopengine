// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Buffer.h"
#include "HSTL/HVector.h"
#include "Resource.h"
#include "String/StaticString.h"

namespace hbe
{

class Engine;
class TaskSystem;

class ResourceManager final
{
public:
	template <typename T>
	using TVector = hbe::HVector<T>;

private:
	class ResourceItem final
	{
	public:
		uint32_t id;
		uint32_t referenceCount;
		StaticString path;
		Buffer buffer;

		ResourceItem()
			: id(0)
			, referenceCount(0)
		{
		}
	};

	class LoadingRequest final
	{
	public:
		uint32_t resourceID;
		StaticString path;

		LoadingRequest()
			: resourceID(0)
		{
		}
	};

	TVector<ResourceItem> resources;
	TVector<ResourceItem*> loadingRequests;

public:
	ResourceManager() noexcept;
	~ResourceManager() noexcept;

	void PostUpdate(Engine& engine) noexcept;
	[[nodiscard]] Resource RequestLoad(StaticString path);
	[[nodiscard]] Resource Load(StaticString path);

private:
	void RequestTasks(TaskSystem& taskSys) noexcept;
};

} // namespace hbe
