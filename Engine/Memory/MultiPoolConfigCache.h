// Created by mooming.go@gmail.com

#pragma once

#include "MultiPoolAllocatorConfig.h"
#include "String/StaticString.h"

namespace hbe
{
	class Buffer;

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
		static StaticString GetClassName();

		size_t Serialize(Buffer& outBuffer);
		bool Deserialize(const Buffer& buffer);

		[[nodiscard]] static auto GetVersion() { return version; }
		[[nodiscard]] auto& GetData() { return data; }
		[[nodiscard]] auto& GetData() const { return data; }

	private:
		void Normalize();
	};

} // namespace hbe
