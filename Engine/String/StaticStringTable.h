// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>
#include <string_view>
#include "Config/EngineConfig.h"
#include "StaticString.h"
#include "StaticStringID.h"

namespace hbe
{

	/// @brief A global table for storing and managing interned static strings.
	class StaticStringTable final
	{
	public:
		using TIndex = size_t;
		static constexpr size_t NumTables = Config::StaticStringNumHashBuckets;

	private:
		template<typename T>
		class Allocator final
		{
		public:
			using value_type = T;

			template<class U>
			struct rebind
			{
				using other = Allocator<U>;
			};

		public:
			Allocator() = default;
			~Allocator() = default;

			T* allocate(std::size_t n) { return (T*) allocate(n * sizeof(T)); }
			void deallocate(T*, std::size_t) {}

			template<class U>
			bool operator==(const Allocator<U>& rhs) const
			{
				return true;
			}

			template<class U>
			bool operator!=(const Allocator<U>& rhs) const
			{
				return false;
			}
		};

		using TTable = std::vector<std::string_view>;

	private:
		mutable std::mutex tableLock;
		TTable tables[NumTables];

	public:
		static StaticStringTable& getInstance();

	public:
		StaticStringTable();
		~StaticStringTable();

		[[nodiscard]] StaticString getName() const;

		[[nodiscard]] StaticStringID Register(const char* str);
		[[nodiscard]] StaticStringID Register(const std::string_view& str);
		[[nodiscard]] const char* get(StaticStringID id) const;

		void printStringTable() const;

	private:
		void registerPredefinedStrings();
		TIndex getTableID(const char* text) const;
		TIndex getTableID(const std::string_view& str) const;

		std::string_view store(const char* text);
		std::string_view store(const std::string_view& str);

		static void* allocate(size_t n);
	};

} // namespace hbe
