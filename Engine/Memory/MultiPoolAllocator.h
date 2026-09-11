// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "HSTL/HVector.h"
#include "PoolAllocator.h"
#include "PoolConfig.h"
#include "String/StaticString.h"

namespace hbe
{

	class MemoryManager;

	/// @brief A multi-pool allocator that manages multiple pools of different block sizes.
	/// @details Uses multiple pools to handle various allocation sizes efficiently.
	/// Blocks are organized into banks that can grow as needed.
	class MultiPoolAllocator final
	{
	public:
		using This = MultiPoolAllocator;

		static constexpr size_t DefaultMinBlock = 16;
		static constexpr size_t DefaultBankUnit = 1024ULL * 1024;
		static constexpr size_t MinNumberOfBlocks = 16;

	private:
		using TInitializerList = std::initializer_list<PoolConfig>;

		TAllocatorID id;
		TAllocatorID parentID;
		StaticString name;
		hbe::HVector<PoolAllocator> banks;
		size_t bankSize;
		size_t minBlock;

	public:
		explicit MultiPoolAllocator(const char* name, size_t allocationUnit = DefaultBankUnit,
									size_t minBlockSize = DefaultMinBlock);
		MultiPoolAllocator(const char* name, TInitializerList initialConfigurations,
						   size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
		~MultiPoolAllocator();

		void* allocate(size_t size);
		void Deallocate(void* ptr, size_t size);

		[[nodiscard]] auto getName() const { return name; }
		[[nodiscard]] auto getID() const { return id; }

		void printUsage() const;

#if PROFILE_ENABLED
		void reportConfiguration() const;
#endif // PROFILE_ENABLED

	private:
		// Generate a new bank to allocae
		void* newBankAllocate(size_t size);

		bool generateBanksByCache(MemoryManager& mmgr);
		[[nodiscard]] size_t getBankIndex(size_t nBytes) const;
		[[nodiscard]] size_t getBankIndex(void* ptr) const;
		[[nodiscard]] size_t calculateBlockSize(size_t requested) const;
		static size_t calculateNumberOfBlocks(size_t bankSize, size_t blockSize);
		void generateBank(size_t blockSize, size_t numberOfBlocks);
	};
} // namespace hbe

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"

namespace hbe
{
	/// @brief Test class for MultiPoolAllocator.
	class MultiPoolAllocatorTest : public TestCollection
	{
	public:
		MultiPoolAllocatorTest() : TestCollection("MultiPoolAllocatorTest") {}

	protected:
		void prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
