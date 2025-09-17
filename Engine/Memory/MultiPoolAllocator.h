// Created by mooming.go@gmail.com

#pragma once

#include "Config/BuildConfig.h"
#include "HSTL/HVector.h"
#include "PoolAllocator.h"
#include "PoolConfig.h"
#include "String/StaticString.h"

namespace hbe
{

	class MemoryManager;

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
		HSTL::HVector<PoolAllocator> banks;
		size_t bankSize;
		size_t minBlock;

	public:
		explicit MultiPoolAllocator(const char* name, size_t allocationUnit = DefaultBankUnit,
									size_t minBlockSize = DefaultMinBlock);
		MultiPoolAllocator(const char* name, TInitializerList initialConfigurations,
						   size_t allocationUnit = DefaultBankUnit, size_t minBlockSize = DefaultMinBlock);
		~MultiPoolAllocator();

		void* Allocate(size_t size);
		void Deallocate(void* ptr, size_t size);

		[[nodiscard]] auto GetName() const { return name; }
		[[nodiscard]] auto GetID() const { return id; }

		void PrintUsage() const;

#if PROFILE_ENABLED
		void ReportConfiguration() const;
#endif // PROFILE_ENABLED

	private:
		// Generate a new bank to allocae
		void* NewBankAllocate(size_t size);

		bool GenerateBanksByCache(MemoryManager& mmgr);
		[[nodiscard]] size_t GetBankIndex(size_t nBytes) const;
		size_t GetBankIndex(void* ptr) const;
		[[nodiscard]] size_t CalculateBlockSize(size_t requested) const;
		static size_t CalculateNumberOfBlocks(size_t bankSize, size_t blockSize);
		void GenerateBank(size_t blockSize, size_t numberOfBlocks);
	};
} // namespace hbe

#ifdef __UNIT_TEST__

#include "Test/TestCollection.h"

namespace hbe
{
	class MultiPoolAllocatorTest : public TestCollection
	{
	public:
		MultiPoolAllocatorTest() : TestCollection("MultiPoolAllocatorTest") {}

	protected:
		void Prepare() override;
	};
} // namespace hbe
#endif //__UNIT_TEST__
