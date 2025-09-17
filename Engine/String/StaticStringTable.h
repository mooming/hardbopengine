// Created by mooming.go@gmail.com, 2022

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>
#include "Config/EngineConfig.h"
#include "StaticString.h"
#include "StaticStringID.h"

namespace hbe
{

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

			T* allocate(std::size_t n) { return (T*) Allocate(n * sizeof(T)); }
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
		static StaticStringTable& GetInstance();

	public:
		StaticStringTable();
		~StaticStringTable();

		StaticString GetName() const;

		StaticStringID Register(const char* str);
		StaticStringID Register(const std::string_view& str);
		const char* Get(StaticStringID id) const;

		void PrintStringTable() const;

	private:
		void RegisterPredefinedStrings();
		TIndex GetTableID(const char* text) const;
		TIndex GetTableID(const std::string_view& str) const;

		std::string_view Store(const char* text);
		std::string_view Store(const std::string_view& str);

		static void* Allocate(size_t n);
	};

} // namespace hbe
