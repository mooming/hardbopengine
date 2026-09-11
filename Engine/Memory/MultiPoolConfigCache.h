// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "MultiPoolAllocatorConfig.h"
#include "String/StaticString.h"

namespace hbe
{
	class Buffer;

	/// @brief Cache for multi-pool allocator configurations.
	/// @details Serializes and deserializes pool configurations.
	class MultiPoolConfigCache final
	{
	public:
		using TVersion = uint32_t;

		template<typename T>
		using TVector = std::vector<T>;
		using TMultiPoolConfigs = TVector<MultiPoolAllocatorConfig>;

	private:
		static constexpr TVersion version = 0;
		TMultiPoolConfigs data;

	public:
		static StaticString getClassName();

		size_t serialize(Buffer& outBuffer);
		bool deserialize(const Buffer& buffer);

		[[nodiscard]] static auto getVersion() { return version; }
		[[nodiscard]] auto& getData() { return data; }
		[[nodiscard]] auto& getData() const { return data; }

	private:
		void normalize();
	};

} // namespace hbe
