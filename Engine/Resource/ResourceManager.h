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

	/// @brief Manages resource loading and lifetime with reference counting.
	class ResourceManager final
	{
		template<typename T>
		using TVector = hbe::HVector<T>;

	private:
		/// @brief Internal storage for a loaded resource with reference counting.
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

} // namespace hbe
