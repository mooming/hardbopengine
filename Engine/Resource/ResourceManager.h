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
	public:
		template<typename T>
		using TVector = hbe::HVector<T>;

		ResourceManager() noexcept;
		~ResourceManager() noexcept;

		void PostUpdate(Engine& engine) noexcept;
		[[nodiscard]] Resource RequestLoad(StaticString path);
		[[nodiscard]] Resource Load(StaticString path);

	private:
		void RequestTasks(TaskSystem& taskSys) noexcept;

		/// @brief Internal storage for a loaded resource with reference counting.
		class ResourceItem final
		{
		public:
			ResourceItem() : id(0), referenceCount(0) {}

			uint32_t id;
			uint32_t referenceCount;
			StaticString path;
			Buffer buffer;
		};

		class LoadingRequest final
		{
		public:
			LoadingRequest() : resourceID(0) {}

			uint32_t resourceID;
			StaticString path;
		};

		TVector<ResourceItem> resources;
		TVector<ResourceItem*> loadingRequests;
	};

} // namespace hbe
