// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "HSTL/HVector.h"
#include "PoolAllocator.h"
#include "PoolConfig.h"
#include "String/StaticString.h"

namespace hbe
{
	/// @brief Thread-safe multi-pool allocator with multiple block sizes.
	/// @details Manages multiple pool allocators of different block sizes.
	/// Uses mutex synchronization for thread safety.
	class ThreadSafeMultiPoolAllocator final
	{
	public:
		using This = ThreadSafeMultiPoolAllocator;

		static constexpr size_t DefaultMinBlock = 16;
		static constexpr size_t DefaultBankUnit = 1024ULL * 1024;
		static constexpr size_t MinNumberOfBlocks = 16;

	private:
		using TInitializerList = std::initializer_list<PoolConfig>;

		TAllocatorID id;
		TAllocatorID parentID;
		StaticString name;

		std::mutex lock;
		hbe::HVector<PoolAllocator> banks;
		size_t bankSize;
		size_t minBlock;

	public:
		explicit ThreadSafeMultiPoolAllocator(const char* name, size_t allocationUnit = DefaultBankUnit,
											  size_t minBlockSize = DefaultMinBlock);
		ThreadSafeMultiPoolAllocator(const char* name, TInitializerList initialConfigurations,
									 size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
		~ThreadSafeMultiPoolAllocator();

		void* allocate(size_t size);
		void Deallocate(void* ptr, size_t size);

		[[nodiscard]] auto getName() const { return name; }
		[[nodiscard]] auto getID() const { return id; }

		void printUsage();

#if PROFILE_ENABLED
		void reportConfiguration();
#endif // PROFILE_ENABLED

	private:
		void* newBankAllocate(size_t size);
		bool generateBanksByCache(const class MemoryManager& mmgr);
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
	/// @brief Test class for ThreadSafeMultiPoolAllocator.
	class ThreadSafeMultiPoolAllocatorTest : public TestCollection
	{
	public:
		ThreadSafeMultiPoolAllocatorTest() : TestCollection("ThreadSafeMultiPoolAllocatorTest") {}

	protected:
		void prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
