// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>

#include "Core/Runnable.h"
#include "Memory/AllocatorID.h"
#include "String/StaticString.h"
#include "Task.h"
#include "TaskID.h"


namespace hbe
{
/// API reference: docs/Core/TaskRegistry/index.html
class TaskRegistry final
{
	using TIndex = Task::TIndex;

private:
	struct Record final
	{
		Task task;
		TaskID successor;

		std::atomic<TaskID::TGeneration> generation;
		std::atomic<bool> inUse;

		std::size_t nextFreeRecord;

		std::byte reservedToCacheLine[24];
	};

	using TBank = Record*;

public:
	static constexpr std::size_t DefaultInitialCapacityRecords = 4096;
	static constexpr std::size_t DefaultGrowByRecords = DefaultInitialCapacityRecords;
	static constexpr std::size_t DefaultMaxCapacityRecords = 0;
	static constexpr std::size_t MaxBanks = 1024;
	static constexpr std::size_t RecordSizeBytes = sizeof(Record);
	static_assert(RecordSizeBytes == 256, "R28 prices a record at 256 bytes; re-measure and re-decide");
	static_assert(RecordSizeBytes % 64 == 0, "Records must start on cache-line boundaries");

private:
	std::mutex registryLock;
	StaticString name;
	TBank banks[MaxBanks];
	std::atomic<std::size_t> bankCount;
	std::size_t recordsPerBank;
	std::size_t maxCapacityRecords;
	std::size_t freeRecordHead;
	std::atomic<std::size_t> usedRecords;
	TAllocatorID bankAllocatorID;

public:
	TaskRegistry() noexcept;
	~TaskRegistry() noexcept;
	TaskRegistry(const TaskRegistry&) = delete;
	TaskRegistry& operator=(const TaskRegistry&) = delete;

	void Initialize(StaticString registryName, std::size_t initialCapacityRecords, std::size_t growByRecords) noexcept;

	void SetSuccessor(TaskID task, TaskID successor) noexcept;
	[[nodiscard]] TaskID GetSuccessor(TaskID task) noexcept;

	[[nodiscard]] TaskID Create(StaticString taskName, TRunnable func, void* userData) noexcept;
	[[nodiscard]] Task* Find(TaskID id) noexcept;
	void Release(TaskID id) noexcept;

	[[nodiscard]] bool Grow() noexcept;

	[[nodiscard]] std::size_t GetCapacity() const noexcept;

	[[nodiscard]] std::size_t GetCount() const noexcept
	{
		return usedRecords.load(std::memory_order::relaxed);
	}

	[[nodiscard]] std::size_t GetGrowBy() const noexcept
	{
		return recordsPerBank;
	}

	[[nodiscard]] std::size_t GetMaxCapacity() const noexcept
	{
		return maxCapacityRecords;
	}

	void SetMaxCapacity(std::size_t maxRecords) noexcept
	{
		maxCapacityRecords = maxRecords;
	}

private:
	TBank AllocateBank() noexcept;
	void DeallocateBank(TBank bank) noexcept;
	[[nodiscard]] Record& GetRecord(std::size_t recordIndex) noexcept;
	[[nodiscard]] Record* FindLiveRecord(TaskID id) noexcept;
	void ReportNoRoom(StaticString taskName) noexcept;
	void ReportRefusedGrowth(std::size_t requestedCapacity) noexcept;
};
} // namespace hbe

#ifdef __TEST__
#include "Test/TestCollection.h"

namespace hbe
{
class TaskRegistryTest final : public TestCollection
{
public:
	TaskRegistryTest()
		: TestCollection("TaskRegistryTest")
	{
	}

protected:
	void Prepare() override;
};
} // namespace hbe
#endif //__TEST__
