// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ThreadSafeMultiPoolAllocator.h"

#include <algorithm>
#include <bit>
#include <map>

#include "Config/BuildConfig.h"
#include "Core/SystemStatistics.h"
#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"
#include "String/StringBuilder.h"


namespace hbe
{
class BankGenerationLog final
{
private:
	const ThreadSafeMultiPoolAllocator* allocator;
	BankGenerationLog* previous;
	size_t blockSize;
	size_t numberOfBlocks;
	bool recorded;

public:
	explicit BankGenerationLog(const ThreadSafeMultiPoolAllocator& inAllocator) noexcept;
	~BankGenerationLog() noexcept;
	BankGenerationLog(const BankGenerationLog&) = delete;
	BankGenerationLog& operator=(const BankGenerationLog&) = delete;

	void RecordBankGeneration(size_t inBlockSize, size_t inNumberOfBlocks) noexcept;
};

static thread_local BankGenerationLog* activeBankGenerationLog = nullptr;

BankGenerationLog::BankGenerationLog(const ThreadSafeMultiPoolAllocator& inAllocator) noexcept
	: allocator(&inAllocator)
	, previous(activeBankGenerationLog)
	, blockSize(0)
	, numberOfBlocks(0)
	, recorded(false)
{
	activeBankGenerationLog = this;
}

BankGenerationLog::~BankGenerationLog() noexcept
{
	activeBankGenerationLog = previous;

	if (!recorded)
	{
		return;
	}

	const auto logName = allocator->GetName();
	auto log = Logger::Get(logName);
	log.Out(ELogLevel::Verbose, [this, logName](auto& ls)
	{ ls << "The bank[" << logName << '_' << blockSize << '_' << numberOfBlocks << "] has been generated. "; });
}

void BankGenerationLog::RecordBankGeneration(size_t inBlockSize, size_t inNumberOfBlocks) noexcept
{
	blockSize = inBlockSize;
	numberOfBlocks = inNumberOfBlocks;
	recorded = true;
}

ThreadSafeMultiPoolAllocator::ThreadSafeMultiPoolAllocator(const char* inName, size_t allocationUnit,
														   size_t minBlockSize)

	: id(InvalidAllocatorID)
	, parentID(InvalidAllocatorID)
	, name(inName)
	, bankSize(allocationUnit)
	, minBlock(minBlockSize)
{
	using namespace hbe;

	parentID = MemoryManager::GetCurrentAllocatorID();

	auto allocFunc = [](void* allocPtr, size_t n) -> void*
	{
		auto* allocator = static_cast<ThreadSafeMultiPoolAllocator*>(allocPtr);

		return allocator->Allocate(n);
	};

	auto deallocFunc = [](void* allocPtr, void* ptr, size_t n)
	{
		auto* allocator = static_cast<ThreadSafeMultiPoolAllocator*>(allocPtr);
		allocator->Deallocate(ptr, n);
	};

	auto& mmgr = MemoryManager::GetInstance();
	GenerateBanksByCache(mmgr);

	size_t capacity = 0;
	for (auto& bank : banks)
	{
		capacity += bank.GetCapacity();
	}

	id = mmgr.RegisterAllocator(this, name, false, capacity, allocFunc, deallocFunc);
}

ThreadSafeMultiPoolAllocator::ThreadSafeMultiPoolAllocator(const char* inName, TInitializerList initialConfigurations,
														   size_t allocationUnit, size_t minBlockSize)

	: id(InvalidAllocatorID)
	, parentID(InvalidAllocatorID)
	, name(inName)
	, bankSize(allocationUnit)
	, minBlock(minBlockSize)
{
	using namespace hbe;

	parentID = MemoryManager::GetCurrentAllocatorID();

	auto allocFunc = [](void* allocPtr, size_t n) -> void*
	{
		auto* allocator = static_cast<ThreadSafeMultiPoolAllocator*>(allocPtr);

		return allocator->Allocate(n);
	};

	auto deallocFunc = [](void* allocPtr, void* ptr, size_t n)
	{
		auto* allocator = static_cast<ThreadSafeMultiPoolAllocator*>(allocPtr);
		allocator->Deallocate(ptr, n);
	};

	auto ConfigureBanks = [&, this]()
	{
		constexpr size_t InlineBufferSize = 128;
		Assert(initialConfigurations.size() <= InlineBufferSize);

		HInlineVector<PoolConfig, InlineBufferSize> vlist;
		vlist.reserve(InlineBufferSize);

		vlist.insert(vlist.end(), initialConfigurations);
		std::sort(vlist.begin(), vlist.end());

		banks.reserve(vlist.size());

		for (auto& config : vlist)
		{
			InlineStringBuilder<1024> str;
			str << name << '_' << config.blockSize << '_' << config.numberOfBlocks;

			banks.emplace_back(str.c_str(), config.blockSize, config.numberOfBlocks);
		}
	};

	ConfigureBanks();

	size_t capacity = 0;
	for (auto& bank : banks)
	{
		capacity += bank.GetCapacity();
	}

	auto& mmgr = MemoryManager::GetInstance();
	id = mmgr.RegisterAllocator(this, name, false, capacity, allocFunc, deallocFunc);
}

ThreadSafeMultiPoolAllocator::~ThreadSafeMultiPoolAllocator()
{
	AllocatorScope scope(parentID);

	auto& mmgr = MemoryManager::GetInstance();

#if PROFILE_ENABLED
	ReportConfiguration();
#endif // PROFILE_ENABLED
	{
		std::lock_guard lockGuard(lock);

#if MEMORY_VERIFICATION_ENABLED
		const auto currentTID = std::this_thread::get_id();
		for (auto& bank : banks)
		{
			auto& allocProxy = mmgr.GetAllocatorProxy(bank.GetID());
			allocProxy.threadId = currentTID;
		}
#endif // MEMORY_VERIFICATION_ENABLED

		banks.clear();
	}

	mmgr.DeregisterAllocator(GetID());
}

void* ThreadSafeMultiPoolAllocator::Allocate(size_t requested)
{
	if (unlikely(requested == 0))
	{
		return nullptr;
	}

	const BankGenerationLog bankGenerationLog(*this);

	void* ptr = nullptr;
#if PROFILE_ENABLED
	size_t allocated = 0;
#endif // PROFILE_ENABLED
	{
		std::lock_guard lockGuard(lock);

		auto index = GetBankIndex(requested);
		if (index >= banks.size())
		{
			return NewBankAllocate(requested);
		}

		auto& bank = banks[index];
		if (unlikely(bank.GetAvailableBlocks() <= 0))
		{
			return NewBankAllocate(requested);
		}

		ptr = bank.Allocate(requested);

#if PROFILE_ENABLED
		allocated = bank.GetBlockSize();
#endif // PROFILE_ENABLED
	}

#if PROFILE_ENABLED
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.ReportAllocation(GetID(), ptr, requested, allocated);
	}
#endif // PROFILE_ENABLED

	return ptr;
}

void ThreadSafeMultiPoolAllocator::Deallocate(void* ptr, size_t size)
{
	if (unlikely(ptr == nullptr))
	{
		Assert(size == 0);

		return;
	}

#if PROFILE_ENABLED
	size_t allocated = 0;
#endif // PROFILE_ENABLED
	bool foreignPointer = false;
	{
		std::lock_guard lockGuard(lock);

		auto index = GetBankIndex(ptr);
		if (index >= banks.size())
		{
			foreignPointer = true;
		}
		else
		{
			auto& bank = banks[index];
			Assert(size <= bank.GetBlockSize());

			bank.Deallocate(ptr, size);

#if PROFILE_ENABLED
			allocated = bank.GetBlockSize();
#endif // PROFILE_ENABLED
		}
	}

	if (unlikely(foreignPointer))
	{
		auto log = Logger::Get(name);
		log.OutFatalError([ptr](auto& ls) { ls << ptr << " is allocated by another allocator."; });

		return;
	}

#if PROFILE_ENABLED
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.ReportDeallocation(GetID(), ptr, size, allocated);
	}
#endif // PROFILE_ENABLED
}

struct BankUsageRecord final
{
	size_t blockSize;
	size_t usage;
	size_t capacity;
	size_t maxUsage;
};

void ThreadSafeMultiPoolAllocator::PrintUsage()
{
#if PROFILE_ENABLED
	AllocatorScope scope(MemoryManager::SystemAllocatorID);

	std::map<size_t, size_t> usageMap;
	HVector<BankUsageRecord> bankUsages;

	auto log = Logger::Get(name, ELogLevel::Info);
	log.Out([this](auto& ls) { ls << hendl << "## MultipoolAllocator(" << name << ") Usage ##"; });
	{
		std::lock_guard lockGuard(lock);

		for (auto& pool : banks)
		{
			bankUsages.push_back(BankUsageRecord{pool.GetBlockSize(), pool.GetUsage(), pool.GetCapacity(),
												 pool.GetUsedBlocksMax() * pool.GetBlockSize()});

			if (pool.GetUsedBlocksMax() > 0)
			{
				auto key = pool.GetBlockSize();
				auto it = usageMap.find(key);

				if (unlikely(it == usageMap.end()))
				{
					usageMap.emplace(key, pool.GetUsedBlocksMax());
				}
				else
				{
					it->second += pool.GetUsedBlocksMax();
				}
			}
		}
	}

	for (const auto& bank : bankUsages)
	{
		log.Out([&bank](auto& ls)
		{
			ls << '[' << bank.blockSize << "] Memory = " << bank.usage << " / " << bank.capacity
			   << ", Max Usage = " << bank.maxUsage;
		});
	}

	StringBuilder args;
	args << "Opt. Args: " << name << " {";

	for (auto& item : usageMap)
	{
		args << " {" << item.first << ", " << item.second << "}, ";
	}

	args << "}";

	log.Out(args.c_str());
#endif // PROFILE_ENABLED
}

#if PROFILE_ENABLED
void ThreadSafeMultiPoolAllocator::ReportConfiguration()
{
	MemoryManager::TPoolConfigs configs;
	configs.reserve(banks.size());

	auto InsertItem = [&configs](const PoolAllocator& alloc)
	{
		auto key = alloc.GetBlockSize();
		auto value = alloc.GetUsedBlocksMax();

		auto pred = [key](const PoolConfig& item) { return item.blockSize == key; };

		auto found = std::ranges::find_if(configs, pred);
		if (found == configs.end())
		{
			configs.emplace_back(key, value);
		}
		else
		{
			found->numberOfBlocks += value;
		}
	};

	size_t peakBlocks = 0;
	size_t reportedBlocks = 0;
	{
		std::lock_guard lockGuard(lock);

		for (auto& bank : banks)
		{
			InsertItem(bank);
			peakBlocks += bank.GetUsedBlocksMax();
		}
	}

	for (const auto& config : configs)
	{
		reportedBlocks += config.numberOfBlocks;
	}

	Assert(reportedBlocks == peakBlocks, "ReportConfiguration: the reported block count ", reportedBlocks,
		   " does not match the summed bank peak ", peakBlocks);

	auto& mmgr = MemoryManager::GetInstance();
	auto uniqueName = GetName();
	mmgr.ReportMultiPoolConfiguration(uniqueName.GetID(), std::move(configs));
}
#endif // PROFILE_ENABLED

void* ThreadSafeMultiPoolAllocator::NewBankAllocate(size_t size)
{
	auto& engine = Engine::Get();
	auto& statistics = engine.GetStatistics();
	statistics.IncFallbackAllocCount();

	auto blockSize = CalculateBlockSize(size);
	auto numBlocks = CalculateNumberOfBlocks(bankSize, blockSize);

	GenerateBank(blockSize, numBlocks);
	auto index = GetBankIndex(size);
	if (unlikely(index >= banks.size()))
	{
		FatalAssert(false);

		return nullptr;
	}

	auto& bank = banks[index];
	auto ptr = bank.Allocate(size);

#if PROFILE_ENABLED
	{
		auto& mmgr = MemoryManager::GetInstance();
		mmgr.ReportAllocation(GetID(), ptr, size, bank.GetBlockSize());
	}
#endif // PROFILE_ENABLED

	return ptr;
}

bool ThreadSafeMultiPoolAllocator::GenerateBanksByCache(const MemoryManager& mmgr)
{
	auto nameID = name.GetID();
	auto& cacheItem = mmgr.LookUpMultiPoolConfig(nameID);
	if (cacheItem.uniqueName != nameID)
	{
		return false;
	}

	auto& configs = cacheItem.configs;
	for (auto& config : configs)
	{
		InlineStringBuilder<1024> str;
		str << name << '_' << config.blockSize << '_' << config.numberOfBlocks;

		banks.emplace_back(str.c_str(), config.blockSize, config.numberOfBlocks);
	}

	return true;
}

size_t ThreadSafeMultiPoolAllocator::GetBankIndex(size_t nBytes) const
{
	nBytes = std::max(minBlock, nBytes);
	const auto doubleSize = nBytes * 2;

	const auto len = banks.size();
	for (size_t i = 0; i < len; ++i)
	{
		auto& bank = banks[i];
		if (nBytes > bank.GetBlockSize())
		{
			continue;
		}

		if (bank.GetAvailableBlocks() <= 0)
		{
			continue;
		}

		if (bank.GetBlockSize() > doubleSize)
		{
			return len;
		}

		return i;
	}

	return len;
}

size_t ThreadSafeMultiPoolAllocator::GetBankIndex(void* ptr) const
{
	size_t index = 0;

	for (auto& bank : banks)
	{
		if (bank.IsMine(ptr))
		{
			return index;
		}

		++index;
	}

	return index;
}

size_t ThreadSafeMultiPoolAllocator::CalculateBlockSize(size_t requested) const
{
	size_t blockSize = (requested + minBlock - 1) / minBlock;
	blockSize = std::bit_ceil(blockSize);
	blockSize *= minBlock;
	blockSize = std::max(minBlock, blockSize);

	return blockSize;
}

size_t ThreadSafeMultiPoolAllocator::CalculateNumberOfBlocks(size_t bankSize, size_t blockSize)
{
	size_t numberOfBlocks = (bankSize + blockSize - 1) / blockSize;

	constexpr size_t megaBytes = 1024 * 1024;
	if (blockSize < megaBytes)
	{
		numberOfBlocks = std::max(MinNumberOfBlocks, numberOfBlocks);
	}
	else
	{
		numberOfBlocks = std::max(static_cast<size_t>(1), numberOfBlocks);
	}

	return numberOfBlocks;
}

void ThreadSafeMultiPoolAllocator::GenerateBank(size_t blockSize, size_t numberOfBlocks)
{
	AllocatorScope scope(parentID);

	Assert(blockSize > 0);
	Assert(numberOfBlocks > 0);

	InlineStringBuilder<1024> str;
	str << name << '_' << blockSize << '_' << numberOfBlocks;
	{
		AllocatorScope allocScope(parentID);
		banks.emplace_back(str.c_str(), blockSize, numberOfBlocks);
		std::sort(banks.begin(), banks.end());
	}

	if (activeBankGenerationLog != nullptr)
	{
		activeBankGenerationLog->RecordBankGeneration(blockSize, numberOfBlocks);
	}

#if PROFILE_ENABLED
	const auto incCapacity = blockSize * numberOfBlocks;
	auto& mmgr = MemoryManager::GetInstance();
	auto& allocProxy = mmgr.GetAllocatorProxy(GetID());
	allocProxy.stats.capacity += incCapacity;
#endif // PROFILE_ENABLED
}
} // namespace hbe

#ifdef __TEST__
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <initializer_list>
#include <optional>
#include <span>
#include <thread>
#include <vector>

#include "AllocatorScope.h"
#include "Core/ScopedTime.h"
#include "Core/Time.h"
#include "MultiPoolAllocator.h"

namespace hbe
{
static constexpr size_t ContentionOpsPerThread = 20000;
static constexpr size_t ContentionBlockSizeCount = 8;
static constexpr size_t ContentionBlockSizes[ContentionBlockSizeCount] = {16, 24, 32, 48, 64, 96, 128, 256};
static constexpr size_t ContentionFreeStride = 7;
static constexpr std::uint32_t ContentionStartSignalTimeoutMilliSecs = 5000;
static constexpr std::uint32_t ContentionWorkerTimeoutMilliSecs = 30000;

struct ContentionWorkerRecord final
{
	time::TTime phaseBegin;
	time::TTime phaseMiddle;
	time::TTime phaseEnd;
	bool sawWaitedSignal;
};

[[nodiscard]] static double DurationToNanoseconds(const time::TDuration& duration) noexcept
{
	return std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(duration).count();
}

[[nodiscard]] static size_t ContentionBlockSizeAt(const size_t opIndex) noexcept
{
	return ContentionBlockSizes[opIndex % ContentionBlockSizeCount];
}

static bool SpinUntilTrueWithDeadline(const std::atomic<bool>& flag, const std::uint32_t timeoutMilliSecs)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliSecs);

	while (!flag.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::yield();
	}

	return flag.load(std::memory_order_acquire);
}

static size_t ResolveContentionThreadCounts(size_t* outThreadCounts)
{
	size_t hardwareThreads = std::thread::hardware_concurrency();
	hardwareThreads = hardwareThreads == 0 ? 1 : hardwareThreads;

	const size_t maxSharedThreads = std::min(hardwareThreads, static_cast<size_t>(8));

	outThreadCounts[0] = 1;
	size_t count = 1;

	for (const size_t candidate : {static_cast<size_t>(2), static_cast<size_t>(4), static_cast<size_t>(8)})
	{
		const size_t resolved = std::min(candidate, maxSharedThreads);

		if (resolved != outThreadCounts[count - 1])
		{
			outThreadCounts[count] = resolved;
			++count;
		}
	}

	return count;
}

template <typename TAllocator>
static void RunAllocateThenDeallocateWorker(TAllocator& allocator, const std::span<void*> slots,
											const std::atomic<bool>& startSignal, ContentionWorkerRecord& outRecord)
{
	const bool sawWaitedSignal = SpinUntilTrueWithDeadline(startSignal, ContentionStartSignalTimeoutMilliSecs);

	const auto begin = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		slots[i] = allocator.Allocate(ContentionBlockSizeAt(i));
	}

	const auto allocated = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		const size_t index = (i * ContentionFreeStride) % slots.size();

		allocator.Deallocate(slots[index], ContentionBlockSizeAt(index));
	}

	outRecord.phaseBegin = begin;
	outRecord.phaseMiddle = allocated;
	outRecord.phaseEnd = time::TStopWatch::now();
	outRecord.sawWaitedSignal = sawWaitedSignal;
}

static void RunMallocThenFreeWorker(const std::span<void*> slots, const std::atomic<bool>& startSignal,
									ContentionWorkerRecord& outRecord)
{
	const bool sawWaitedSignal = SpinUntilTrueWithDeadline(startSignal, ContentionStartSignalTimeoutMilliSecs);

	const auto begin = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		slots[i] = std::malloc(ContentionBlockSizeAt(i));
	}

	const auto allocated = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		const size_t index = (i * ContentionFreeStride) % slots.size();

		std::free(slots[index]);
	}

	outRecord.phaseBegin = begin;
	outRecord.phaseMiddle = allocated;
	outRecord.phaseEnd = time::TStopWatch::now();
	outRecord.sawWaitedSignal = sawWaitedSignal;
}

static void RunHandoffAllocateWorker(ThreadSafeMultiPoolAllocator& allocator, const std::span<void*> slots,
									 const std::atomic<bool>& startSignal, std::atomic<bool>& batchReady,
									 ContentionWorkerRecord& outRecord)
{
	const bool sawWaitedSignal = SpinUntilTrueWithDeadline(startSignal, ContentionStartSignalTimeoutMilliSecs);

	outRecord.phaseBegin = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		slots[i] = allocator.Allocate(ContentionBlockSizeAt(i));
	}

	outRecord.phaseMiddle = time::TStopWatch::now();
	outRecord.phaseEnd = outRecord.phaseMiddle;
	outRecord.sawWaitedSignal = sawWaitedSignal;

	batchReady.store(true, std::memory_order_release);
}

static void RunHandoffDeallocateWorker(ThreadSafeMultiPoolAllocator& allocator, const std::span<void*> slots,
									   const std::atomic<bool>& batchReady, ContentionWorkerRecord& outRecord)
{
	const bool sawWaitedSignal = WaitUntil([&batchReady] { return batchReady.load(std::memory_order_acquire); },
										   ContentionWorkerTimeoutMilliSecs);

	outRecord.phaseBegin = time::TStopWatch::now();
	outRecord.phaseMiddle = outRecord.phaseBegin;

	if (sawWaitedSignal)
	{
		for (size_t i = 0; i < slots.size(); ++i)
		{
			const size_t index = (i * ContentionFreeStride) % slots.size();

			allocator.Deallocate(slots[index], ContentionBlockSizeAt(index));
		}
	}

	outRecord.phaseEnd = time::TStopWatch::now();
	outRecord.sawWaitedSignal = sawWaitedSignal;
}

static void RunBankGrowthLatencyWorker(ThreadSafeMultiPoolAllocator& allocator, const std::span<void*> slots,
									   const std::span<std::int64_t> opNanos, const std::atomic<bool>& startSignal,
									   ContentionWorkerRecord& outRecord)
{
	const bool sawWaitedSignal = SpinUntilTrueWithDeadline(startSignal, ContentionStartSignalTimeoutMilliSecs);

	const auto begin = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		const auto opBegin = time::TStopWatch::now();

		slots[i] = allocator.Allocate(ContentionBlockSizeAt(i));

		const auto opEnd = time::TStopWatch::now();

		opNanos[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opBegin).count();
	}

	const auto allocated = time::TStopWatch::now();

	for (size_t i = 0; i < slots.size(); ++i)
	{
		const size_t index = (i * ContentionFreeStride) % slots.size();

		allocator.Deallocate(slots[index], ContentionBlockSizeAt(index));
	}

	outRecord.phaseBegin = begin;
	outRecord.phaseMiddle = allocated;
	outRecord.phaseEnd = time::TStopWatch::now();
	outRecord.sawWaitedSignal = sawWaitedSignal;
}

template <typename TWorker>
static bool RunGatedWorkerBatch(TestCollection& owner, TestCollection::TLogOut& ls, std::vector<void*>& slots,
								std::vector<std::int64_t>& opNanos, const size_t opsPerWorker, TWorker worker,
								double& outNsPerOp)
{
	const size_t numWorkers = slots.size() / opsPerWorker;

	std::vector<ContentionWorkerRecord> records(numWorkers);
	std::vector<std::thread> workers;
	workers.reserve(numWorkers);

	std::atomic<bool> startSignal{false};
	std::atomic<size_t> finishedWorkers{0};

	for (size_t workerIndex = 0; workerIndex < numWorkers; ++workerIndex)
	{
		workers.emplace_back(
				[worker, &slots, &opNanos, &records, &startSignal, &finishedWorkers, workerIndex, opsPerWorker]
		{
			const std::span<void*> slotSlice(slots.data() + workerIndex * opsPerWorker, opsPerWorker);
			const std::span<std::int64_t> nanosSlice =
					opNanos.empty()
							? std::span<std::int64_t>{}
							: std::span<std::int64_t>(opNanos.data() + workerIndex * opsPerWorker, opsPerWorker);

			worker(workerIndex, slotSlice, nanosSlice, startSignal, records[workerIndex]);

			finishedWorkers.fetch_add(1, std::memory_order_acq_rel);
		});
	}

	startSignal.store(true, std::memory_order_release);

	const bool allWorkersFinished = WaitUntil([&finishedWorkers, numWorkers]
	{ return finishedWorkers.load(std::memory_order_acquire) >= numWorkers; }, ContentionWorkerTimeoutMilliSecs);

	for (auto& workerThread : workers)
	{
		workerThread.join();
	}

	if (!allWorkersFinished)
	{
		ls << "The worker batch did not finish before its " << ContentionWorkerTimeoutMilliSecs
		   << " ms deadline, so its figures are not reported." << owner.lferr;

		return false;
	}

	auto earliestBegin = records.front().phaseBegin;
	auto latestEnd = records.front().phaseEnd;
	bool missedStartSignal = false;

	for (const auto& record : records)
	{
		missedStartSignal = missedStartSignal || !record.sawWaitedSignal;
		earliestBegin = std::min(earliestBegin, record.phaseBegin);
		latestEnd = std::max(latestEnd, record.phaseEnd);
	}

	if (missedStartSignal)
	{
		ls << "A worker did not observe the start signal before its " << ContentionStartSignalTimeoutMilliSecs
		   << " ms deadline, so its figures are not reported." << owner.lferr;

		return false;
	}

	outNsPerOp = DurationToNanoseconds(latestEnd - earliestBegin) / static_cast<double>(slots.size());

	return true;
}

[[nodiscard]] static double RunSharedWorkloadPass(TestCollection& owner, TestCollection::TLogOut& ls,
												  const char* allocatorName, const size_t numThreads)
{
	ThreadSafeMultiPoolAllocator sharedAllocator(allocatorName, {{16, 16}, {32, 16}, {64, 16}, {128, 16}, {256, 16}});

	std::vector<void*> slots(numThreads * ContentionOpsPerThread);
	std::vector<std::int64_t> noPerOpNanos;
	double nsPerOp = 0.0;

	const bool measured = RunGatedWorkerBatch(
			owner, ls, slots, noPerOpNanos, ContentionOpsPerThread,
			[&sharedAllocator](const size_t, const std::span<void*> slotSlice, const std::span<std::int64_t>,
							   const std::atomic<bool>& startSignal, ContentionWorkerRecord& record)
	{ RunAllocateThenDeallocateWorker(sharedAllocator, slotSlice, startSignal, record); },
			nsPerOp);

	if (!measured)
	{
		return -1.0;
	}

	return nsPerOp;
}

static void RunSharedInstanceScalingMeasurement(TestCollection& owner, TestCollection::TLogOut& ls)
{
	ls << "M1 figures are direct Allocate/Deallocate calls on the allocator object; the MemoryManager proxy "
		  "dispatch is excluded. nsPerOp counts one allocate and one free as one operation."
	   << owner.lf;

	size_t threadCounts[4];
	const size_t configCount = ResolveContentionThreadCounts(threadCounts);

	double nsPerOpSingleThread = 0.0;

	for (size_t configIndex = 0; configIndex < configCount; ++configIndex)
	{
		const size_t numThreads = threadCounts[configIndex];

		const char* allocatorName = numThreads == 1	  ? "M1 Shared One"
									: numThreads == 2 ? "M1 Shared Two"
									: numThreads == 4 ? "M1 Shared Four"
													  : "M1 Shared Peak";

		const double nsPerOp = RunSharedWorkloadPass(owner, ls, allocatorName, numThreads);

		if (nsPerOp <= 0.0)
		{
			continue;
		}

		if (numThreads == 1)
		{
			nsPerOpSingleThread = nsPerOp;
		}

		if (nsPerOpSingleThread <= 0.0)
		{
			continue;
		}

		const double scalingEfficiency = nsPerOpSingleThread / (nsPerOp * static_cast<double>(numThreads));

		ls << "M1 shared N=" << numThreads << " ops=" << numThreads * ContentionOpsPerThread
		   << " nsPerOp=" << std::fixed << std::setprecision(1) << nsPerOp << " eff=" << std::setprecision(3)
		   << scalingEfficiency << owner.lf;

		if (numThreads > 1 && scalingEfficiency < 0.5)
		{
			ls << "M1 scaling efficiency " << std::setprecision(3) << scalingEfficiency << " at " << numThreads
			   << " threads is below 0.5, which reads as the lock being the cost rather than the allocator."
			   << owner.lfwarn;
		}
	}
}

[[nodiscard]] static double RunMallocWorkloadPass(TestCollection& owner, TestCollection::TLogOut& ls,
												  const size_t numThreads)
{
	std::vector<void*> slots(numThreads * ContentionOpsPerThread);
	std::vector<std::int64_t> noPerOpNanos;
	double nsPerOp = 0.0;

	const bool measured =
			RunGatedWorkerBatch(owner, ls, slots, noPerOpNanos, ContentionOpsPerThread,
								[](const size_t, const std::span<void*> slotSlice, const std::span<std::int64_t>,
								   const std::atomic<bool>& startSignal, ContentionWorkerRecord& record)
	{ RunMallocThenFreeWorker(slotSlice, startSignal, record); },
								nsPerOp);

	if (!measured)
	{
		return -1.0;
	}

	return nsPerOp;
}

[[nodiscard]] static double RunPrivatePoolWorkloadPass(TestCollection& owner, TestCollection::TLogOut& ls,
													   const size_t numThreads)
{
	std::deque<MultiPoolAllocator> privatePools;

	for (size_t poolIndex = 0; poolIndex < numThreads; ++poolIndex)
	{
		privatePools.emplace_back(
				"M2 Private Pool",
				std::initializer_list<PoolConfig>{{16, 5000}, {32, 5000}, {64, 5000}, {128, 5000}, {256, 5000}});
	}

	std::vector<void*> slots(numThreads * ContentionOpsPerThread);
	std::vector<std::int64_t> noPerOpNanos;
	double nsPerOp = 0.0;

	const bool measured = RunGatedWorkerBatch(
			owner, ls, slots, noPerOpNanos, ContentionOpsPerThread,
			[&privatePools](const size_t workerIndex, const std::span<void*> slotSlice, const std::span<std::int64_t>,
							const std::atomic<bool>& startSignal, ContentionWorkerRecord& record)
	{ RunAllocateThenDeallocateWorker(privatePools[workerIndex], slotSlice, startSignal, record); },
			nsPerOp);

	if (!measured)
	{
		return -1.0;
	}

	return nsPerOp;
}

static void RunBaselineComparisonMeasurement(TestCollection& owner, TestCollection::TLogOut& ls)
{
	ls << "M2 figures are direct calls on each allocator object; the MemoryManager proxy dispatch is excluded. "
		  "The shared column is a fresh run of the M1 workload inside this testlet. The sharded pass pre-sizes every "
		  "private pool to 5000 blocks per size class, so no bank grows during its timed region."
	   << owner.lf;

	size_t threadCounts[4];
	const size_t configCount = ResolveContentionThreadCounts(threadCounts);

	for (size_t configIndex = 0; configIndex < configCount; ++configIndex)
	{
		const size_t numThreads = threadCounts[configIndex];
		const size_t totalOps = numThreads * ContentionOpsPerThread;

		const char* sharedAllocatorName = numThreads == 1	? "M2 Shared One"
										  : numThreads == 2 ? "M2 Shared Two"
										  : numThreads == 4 ? "M2 Shared Four"
															: "M2 Shared Peak";

		const double sharedNsPerOp = RunSharedWorkloadPass(owner, ls, sharedAllocatorName, numThreads);
		const double mallocNsPerOp = RunMallocWorkloadPass(owner, ls, numThreads);
		const double shardedNsPerOp = RunPrivatePoolWorkloadPass(owner, ls, numThreads);

		if (sharedNsPerOp <= 0.0 || mallocNsPerOp <= 0.0 || shardedNsPerOp <= 0.0)
		{
			continue;
		}

		ls << "M2 malloc N=" << numThreads << " ops=" << totalOps << " nsPerOp=" << std::fixed << std::setprecision(1)
		   << mallocNsPerOp << " sharedNsPerOp=" << sharedNsPerOp << " sharedOverMalloc=" << std::setprecision(3)
		   << sharedNsPerOp / mallocNsPerOp << owner.lf;

		ls << "M2 sharded N=" << numThreads << " ops=" << totalOps << " nsPerOp=" << std::fixed << std::setprecision(1)
		   << shardedNsPerOp << " sharedNsPerOp=" << sharedNsPerOp << " sharedOverSharded=" << std::setprecision(3)
		   << sharedNsPerOp / shardedNsPerOp << owner.lf;

		if (sharedNsPerOp > mallocNsPerOp)
		{
			ls << "M2 the shared instance is slower than std::malloc at " << numThreads
			   << " threads: " << std::setprecision(3) << sharedNsPerOp / mallocNsPerOp
			   << " times the nanoseconds per operation." << owner.lfwarn;
		}
	}
}

static bool RunSameThreadFreePass(TestCollection& owner, TestCollection::TLogOut& ls,
								  ThreadSafeMultiPoolAllocator& allocator, std::vector<void*>& slots,
								  double& outAllocNsPerOp, double& outFreeNsPerOp)
{
	ContentionWorkerRecord record;
	std::atomic<bool> startSignal{false};
	std::atomic<bool> workerFinished{false};

	std::thread workerThread([&allocator, &slots, &startSignal, &workerFinished, &record]
	{
		RunAllocateThenDeallocateWorker(allocator, std::span<void*>(slots), startSignal, record);

		workerFinished.store(true, std::memory_order_release);
	});

	startSignal.store(true, std::memory_order_release);

	const bool sawFinished = WaitUntil([&workerFinished] { return workerFinished.load(std::memory_order_acquire); },
									   ContentionWorkerTimeoutMilliSecs);

	workerThread.join();

	if (!sawFinished || !record.sawWaitedSignal)
	{
		ls << "M3 the same-thread pass did not reach its deadlines, so its figures are not reported." << owner.lferr;

		return false;
	}

	outAllocNsPerOp = DurationToNanoseconds(record.phaseMiddle - record.phaseBegin) / ContentionOpsPerThread;
	outFreeNsPerOp = DurationToNanoseconds(record.phaseEnd - record.phaseMiddle) / ContentionOpsPerThread;

	return true;
}

static bool RunCrossThreadHandoffPass(TestCollection& owner, TestCollection::TLogOut& ls,
									  ThreadSafeMultiPoolAllocator& allocator, std::vector<void*>& slots,
									  double& outAllocNsPerOp, double& outFreeNsPerOp)
{
	ContentionWorkerRecord allocateRecord;
	ContentionWorkerRecord deallocateRecord;
	std::atomic<bool> startSignal{false};
	std::atomic<bool> batchReady{false};
	std::atomic<size_t> finishedWorkers{0};

	std::thread allocateThread([&allocator, &slots, &startSignal, &batchReady, &finishedWorkers, &allocateRecord]
	{
		RunHandoffAllocateWorker(allocator, std::span<void*>(slots), startSignal, batchReady, allocateRecord);

		finishedWorkers.fetch_add(1, std::memory_order_acq_rel);
	});

	std::thread deallocateThread([&allocator, &slots, &batchReady, &finishedWorkers, &deallocateRecord]
	{
		RunHandoffDeallocateWorker(allocator, std::span<void*>(slots), batchReady, deallocateRecord);

		finishedWorkers.fetch_add(1, std::memory_order_acq_rel);
	});

	startSignal.store(true, std::memory_order_release);

	const bool sawFinished = WaitUntil([&finishedWorkers]
	{ return finishedWorkers.load(std::memory_order_acquire) >= 2; }, ContentionWorkerTimeoutMilliSecs);

	allocateThread.join();
	deallocateThread.join();

	if (!sawFinished || !allocateRecord.sawWaitedSignal || !deallocateRecord.sawWaitedSignal)
	{
		ls << "M3 the cross-thread handoff pass did not reach its deadlines, so its figures are not reported."
		   << owner.lferr;

		return false;
	}

	outAllocNsPerOp =
			DurationToNanoseconds(allocateRecord.phaseMiddle - allocateRecord.phaseBegin) / ContentionOpsPerThread;
	outFreeNsPerOp =
			DurationToNanoseconds(deallocateRecord.phaseEnd - deallocateRecord.phaseBegin) / ContentionOpsPerThread;

	return true;
}

static void RunCrossThreadFreeMeasurement(TestCollection& owner, TestCollection::TLogOut& ls)
{
	ls << "M3 figures are direct Allocate/Deallocate calls on the allocator object; the MemoryManager proxy "
		  "dispatch is excluded."
	   << owner.lf;

	std::vector<void*> slots(ContentionOpsPerThread);

	double sameThreadAllocNsPerOp = 0.0;
	double sameThreadFreeNsPerOp = 0.0;
	double crossThreadAllocNsPerOp = 0.0;
	double crossThreadFreeNsPerOp = 0.0;

	ThreadSafeMultiPoolAllocator sameThreadAllocator("M3 Same Thread",
													 {{16, 16}, {32, 16}, {64, 16}, {128, 16}, {256, 16}});

	const bool sameThreadMeasured =
			RunSameThreadFreePass(owner, ls, sameThreadAllocator, slots, sameThreadAllocNsPerOp, sameThreadFreeNsPerOp);

	ThreadSafeMultiPoolAllocator handoffAllocator("M3 Handoff", {{16, 16}, {32, 16}, {64, 16}, {128, 16}, {256, 16}});

	const bool crossThreadMeasured = RunCrossThreadHandoffPass(owner, ls, handoffAllocator, slots,
															   crossThreadAllocNsPerOp, crossThreadFreeNsPerOp);

	if (!sameThreadMeasured || !crossThreadMeasured)
	{
		return;
	}

	ls << "M3 handoff ops=" << ContentionOpsPerThread << " sameThreadAllocNsPerOp=" << std::fixed
	   << std::setprecision(1) << sameThreadAllocNsPerOp << " sameThreadFreeNsPerOp=" << sameThreadFreeNsPerOp
	   << " crossThreadAllocNsPerOp=" << crossThreadAllocNsPerOp << " crossThreadFreeNsPerOp=" << crossThreadFreeNsPerOp
	   << " crossFreeOverSameFree=" << std::setprecision(3) << crossThreadFreeNsPerOp / sameThreadFreeNsPerOp
	   << owner.lf;
}

static void RunBankGrowthLatencyMeasurement(TestCollection& owner, TestCollection::TLogOut& ls)
{
	ls << "M4 times every individual Allocate call while the size classes are exhausted and banks grow; the "
		  "MemoryManager proxy dispatch is excluded."
	   << owner.lf;

	size_t threadCounts[4];
	const size_t configCount = ResolveContentionThreadCounts(threadCounts);
	const size_t numThreads = threadCounts[configCount - 1];

	ThreadSafeMultiPoolAllocator sharedAllocator("M4 Growth", {{16, 16}, {32, 16}, {64, 16}, {128, 16}, {256, 16}});

	std::vector<void*> slots(numThreads * ContentionOpsPerThread);
	std::vector<std::int64_t> opNanos(numThreads * ContentionOpsPerThread);
	double nsPerOp = 0.0;

	const bool measured = RunGatedWorkerBatch(
			owner, ls, slots, opNanos, ContentionOpsPerThread,
			[&sharedAllocator](const size_t, const std::span<void*> slotSlice, const std::span<std::int64_t> nanosSlice,
							   const std::atomic<bool>& startSignal, ContentionWorkerRecord& record)
	{ RunBankGrowthLatencyWorker(sharedAllocator, slotSlice, nanosSlice, startSignal, record); },
			nsPerOp);

	if (!measured)
	{
		return;
	}

	const size_t opCount = opNanos.size();
	const auto medianPosition = opNanos.begin() + static_cast<std::ptrdiff_t>(opCount / 2);
	const auto worstOnePercentPosition = opNanos.begin() + static_cast<std::ptrdiff_t>(opCount * 99 / 100);

	const std::int64_t maxNanos = *std::max_element(opNanos.begin(), opNanos.end());

	std::nth_element(opNanos.begin(), medianPosition, opNanos.end());
	const std::int64_t medianNanos = *medianPosition;

	std::nth_element(opNanos.begin(), worstOnePercentPosition, opNanos.end());
	const std::int64_t worstOnePercentNanos = *worstOnePercentPosition;

	ls << "M4 growth N=" << numThreads << " ops=" << opCount << " typicalNs=" << medianNanos
	   << " p99Ns=" << worstOnePercentNanos << " maxNs=" << maxNanos << owner.lf;
}

void ThreadSafeMultiPoolAllocatorTest::Prepare()
{
	AddTest("Basic Construction", [this](auto& ls)
	{
		ThreadSafeMultiPoolAllocator allocator(
				"TC0 ThreadSafeMultiPoolAlloc",
				{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

#if PROFILE_ENABLED
		auto& mmgr = MemoryManager::GetInstance();
		auto stat = mmgr.GetAllocatorStat(allocator.GetID());
		ls << "Capacity = " << stat.capacity << lf;
#else
		ls << "ThreadSafeMultiPoolAllocator has been created." << lf;
#endif // PROFILE_ENABLED

		AllocatorScope scope(allocator);
	});

	AddTest("Allocation 0", [this](auto& ls)
	{
		ThreadSafeMultiPoolAllocator allocator(
				"TC1 ThreadSafeMultiPoolAlloc",
				{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

		auto& mmgr = MemoryManager::GetInstance();
#if PROFILE_ENABLED
		auto stat = mmgr.GetAllocatorStat(allocator.GetID());
		ls << "Capacity = " << stat.capacity << lf;
#else
		ls << "ThreadSafeMultiPoolAlloc has been created" << lf;
#endif // PROFILE_ENABLED

		AllocatorScope scope(allocator);
		mmgr.Allocate(0);
	});

	AddTest("Multiple Allocations & Fallback", [this](auto& ls)
	{
		ThreadSafeMultiPoolAllocator allocator(
				"TC2 ThreadSafeMultiPoolAlloc",
				{{64, 1024}, {128, 1024}, {256, 1024}, {512, 1024}, {1024, 1024}, {2048, 1024}, {4096, 1024}});

		auto& mmgr = MemoryManager::GetInstance();

#if PROFILE_ENABLED
		{
			auto stat = mmgr.GetAllocatorStat(allocator.GetID());
			ls << "Capacity = " << stat.capacity << lf;
		}
#else
		ls << "ThreadSafeMultiPoolAlloc has been created" << lf;
#endif // PROFILE_ENABLED

		AllocatorScope scope(allocator);

		void* pointers[] = {mmgr.Allocate(0),	 mmgr.Allocate(8),	  mmgr.Allocate(16),  mmgr.Allocate(32),
							mmgr.Allocate(97),	 mmgr.Allocate(110),  mmgr.Allocate(140), mmgr.Allocate(270),
							mmgr.Allocate(4032), mmgr.Allocate(5000), mmgr.Allocate(8000)};

		for (auto ptr : pointers)
		{
			mmgr.Deallocate(ptr, 0);
		}

#if PROFILE_ENABLED
		auto stat = mmgr.GetAllocatorStat(allocator.GetID());
		ls << "Capacity = " << stat.capacity << lf;

		if (stat.fallbackCount != 2)
		{
			ls << "Fallback count mismatched. FallbackCount = " << stat.fallbackCount << ", but 2 expected." << lferr;
		}
#endif // PROFILE_ENABLED
	});

	AddTest("Performance", [this](auto& ls)
	{
		time::TDuration heDuration;
		time::TDuration stdDuration;

		constexpr size_t repeatCount = 100000;

		ThreadSafeMultiPoolAllocator allocator("PerfTestMultiPoolAlloc");
		{
			AllocatorScope allocScope(allocator);
			time::ScopedTime timer(heDuration);

			int strLen = 1;
			hbe::HVector<hbe::HString> v;
			for (size_t i = 0; i < repeatCount; ++i)
			{
				auto& str = v.emplace_back();
				for (int j = 0; j < strLen; ++j)
				{
					char ch = 'a' + j;
					if (ch == '\0')
					{
						ch = 'a';
						strLen = 1;
					}

					str.push_back(ch);
				}

				++strLen;
			}
		}
		{
			time::ScopedTime timer(stdDuration);

			int strLen = 1;
			std::vector<std::string> v;
			for (size_t i = 0; i < repeatCount; ++i)
			{
				auto& str = v.emplace_back();
				for (int j = 0; j < strLen; ++j)
				{
					char ch = 'a' + j;
					if (ch == '\0')
					{
						ch = 'a';
						strLen = 1;
					}

					str.push_back(ch);
				}

				++strLen;
			}
		}

		float heSec = time::ToFloat(heDuration);
		float stdSec = time::ToFloat(stdDuration);

		ls << "Performance: ThreadSafeMultiPoolAllocator = " << heSec << " sec, std malloc = " << stdSec << " sec"
		   << lf;

		if (heSec > stdSec)
		{
			ls << "ThreadSafeMultiPoolAllocator is slower than std malloc."
			   << " ThreadSafeMultiPoolAllocator  = " << heSec << " sec, std malloc = " << stdSec << " sec" << lfwarn;
		}
	});

	AddTest("Pool Buffer Returns To Its Recorded Parent", [this](auto& ls)
	{
		ThreadSafeMultiPoolAllocator parent("TC4 Pool Parent", {{1024, 16}});
		ThreadSafeMultiPoolAllocator unrelated("TC4 Pool Unrelated", {{1024, 16}});
		bool allocated = false;
		{
			AllocatorScope parentScope(parent);
			std::optional<PoolAllocator> pool;
			pool.emplace("TC4 Pool Owner", 64, 8);
			allocated = pool->Allocate(32) != nullptr;
			{
				AllocatorScope unrelatedScope(unrelated);
				pool.reset();
			}
		}

		if (!allocated)
		{
			ls << "The pool handed out no block, so its buffer was never exercised." << lferr;

			return;
		}

		ls << "The pool buffer went to its recorded parent although the ambient scope was another allocator." << lf;
	});

	AddTest("Print Usage Under Its Own Allocator Scope", [this](auto& ls)
	{
		ThreadSafeMultiPoolAllocator allocator("TC5 PrintUsage", {{64, 16}, {128, 16}});

		AllocatorScope scope(allocator);

		auto* block = allocator.Allocate(64);
		if (block == nullptr)
		{
			ls << "The allocator handed out no block, so PrintUsage had no usage to report." << lferr;

			return;
		}

		allocator.PrintUsage();
		allocator.Deallocate(block, 64);

		ls << "PrintUsage returned although the allocator under test was the ambient scope." << lf;
	});

	AddTest("Shared Instance Contention Scaling M1",
			[this](auto& ls) { RunSharedInstanceScalingMeasurement(*this, ls); });

	AddTest("Malloc And Sharded Pool Baselines M2", [this](auto& ls) { RunBaselineComparisonMeasurement(*this, ls); });

	AddTest("Cross Thread Free Cost M3", [this](auto& ls) { RunCrossThreadFreeMeasurement(*this, ls); });

	AddTest("Bank Growth Latency Spike M4", [this](auto& ls) { RunBankGrowthLatencyMeasurement(*this, ls); });
}
} // namespace hbe
#endif //__TEST__
