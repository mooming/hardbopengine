// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "TaskRegistry.h"

#include <new>

#include "Core/Debug.h"
#include "Log/Logger.h"
#include "Memory/MemoryManager.h"


namespace hbe
{
TaskRegistry::TaskRegistry() noexcept
	: name("None")
	, bankCount(0)
	, recordsPerBank(DefaultGrowByRecords)
	, maxCapacityRecords(DefaultMaxCapacityRecords)
	, freeRecordHead(TaskID::NullIndex)
	, usedRecords(0)
	, bankAllocatorID(MemoryManager::GetCurrentAllocatorID())
{
	for (auto& bank : banks)
	{
		bank = nullptr;
	}
}

void TaskRegistry::Initialize(StaticString registryName, std::size_t initialCapacityRecords,
							  std::size_t growByRecords) noexcept
{
	name = registryName;
	recordsPerBank = growByRecords > 0 ? growByRecords : DefaultGrowByRecords;
	maxCapacityRecords = DefaultMaxCapacityRecords;
	freeRecordHead = TaskID::NullIndex;
	bankAllocatorID = MemoryManager::GetCurrentAllocatorID();

	Assert(growByRecords > 0, "A task registry growing by 0 records could never grow.");
	Assert(initialCapacityRecords % recordsPerBank == 0, "Task registry", name.c_str(), "was asked for",
		   initialCapacityRecords, "records, which is not a whole number of banks of", recordsPerBank,
		   "so it holds fewer than that.");

	const std::size_t banksToAllocate = initialCapacityRecords / recordsPerBank;
	for (std::size_t bank = 0; bank < banksToAllocate; ++bank)
	{
		if (AllocateBank() == nullptr)
		{
			break;
		}
	}

	auto log = Logger::Get(name);
	log.Out([this, name = name](auto& ls)
	{ ls << name.c_str() << " is initialized with " << GetCapacity() << " task records."; });
}

TaskRegistry::~TaskRegistry() noexcept
{
	const std::size_t allocatedBanks = bankCount.load(std::memory_order::relaxed);
	for (std::size_t bank = 0; bank < allocatedBanks; ++bank)
	{
		DeallocateBank(banks[bank]);
	}

	bankCount.store(0, std::memory_order::relaxed);
}

TaskID TaskRegistry::Create(StaticString taskName, TRunnable func, void* userData) noexcept
{
	std::scoped_lock<std::mutex> lock(registryLock);

	if (freeRecordHead == TaskID::NullIndex)
	{
		ReportNoRoom(taskName);

		return {};
	}

	const std::size_t recordIndex = freeRecordHead;
	Record& record = GetRecord(recordIndex);
	freeRecordHead = record.nextFreeRecord;

	const TaskID::TGeneration generation = record.generation.load(std::memory_order::relaxed) + 1;
	record.generation.store(generation, std::memory_order::relaxed);
	record.successor = {};
	record.task.LoadIntoRecord(TaskID{recordIndex, generation}, taskName, func, userData);
	record.inUse.store(true, std::memory_order::release);
	usedRecords.fetch_add(1, std::memory_order::relaxed);

	return TaskID{recordIndex, generation};
}

Task* TaskRegistry::Find(TaskID id) noexcept
{
	if (id.IsNull())
	{
		return nullptr;
	}

	const std::size_t bankIndex = id.index / recordsPerBank;
	if (bankIndex >= bankCount.load(std::memory_order::acquire))
	{
		return nullptr;
	}

	Record& record = banks[bankIndex][id.index % recordsPerBank];
	if (!record.inUse.load(std::memory_order::acquire))
	{
		return nullptr;
	}

	if (record.generation.load(std::memory_order::acquire) != id.generation)
	{
		return nullptr;
	}

	return &record.task;
}

void TaskRegistry::SetSuccessor(TaskID task, TaskID successor) noexcept
{
	Record* record = FindLiveRecord(task);
	if (record == nullptr)
	{
		auto log = Logger::Get(name);
		log.OutError([name = name, index = task.index, generation = task.generation](auto& ls)
		{
			ls << name.c_str() << " was asked to record a successor for task record " << index << " generation "
			   << generation << ", which it is not tracking, so nothing will be dispatched when that join closes. ";
		});

		return;
	}

	record->successor = successor;
}

TaskID TaskRegistry::GetSuccessor(TaskID task) noexcept
{
	const Record* record = FindLiveRecord(task);

	return record == nullptr ? TaskID{} : record->successor;
}

void TaskRegistry::Release(TaskID id) noexcept
{
	std::scoped_lock<std::mutex> lock(registryLock);

	Record* record = id.IsNull() ? nullptr : FindLiveRecord(id);
	if (record == nullptr)
	{
		auto log = Logger::Get(name);
		log.OutError([name = name, index = id.index, generation = id.generation](auto& ls)
		{
			ls << name.c_str() << " was asked to release task record " << index << " generation " << generation
			   << ", which is not a task it is tracking. Nothing was freed, so this is either a double release or"
			   << " an ID from a registry that did not issue it. ";
		});

		return;
	}

	record->inUse.store(false, std::memory_order::release);
	record->nextFreeRecord = freeRecordHead;
	freeRecordHead = id.index;
	usedRecords.fetch_sub(1, std::memory_order::relaxed);
}

bool TaskRegistry::Grow() noexcept
{
	std::scoped_lock<std::mutex> lock(registryLock);

	return AllocateBank() != nullptr;
}

std::size_t TaskRegistry::GetCapacity() const noexcept
{
	return bankCount.load(std::memory_order::relaxed) * recordsPerBank;
}

TaskRegistry::Record& TaskRegistry::GetRecord(std::size_t recordIndex) noexcept
{
	return banks[recordIndex / recordsPerBank][recordIndex % recordsPerBank];
}

TaskRegistry::Record* TaskRegistry::FindLiveRecord(TaskID id) noexcept
{
	if (id.IsNull())
	{
		return nullptr;
	}

	const std::size_t bankIndex = id.index / recordsPerBank;
	if (bankIndex >= bankCount.load(std::memory_order::relaxed))
	{
		return nullptr;
	}

	Record& record = banks[bankIndex][id.index % recordsPerBank];
	if (!record.inUse.load(std::memory_order::relaxed) ||
		record.generation.load(std::memory_order::relaxed) != id.generation)
	{
		return nullptr;
	}

	return &record;
}

TaskRegistry::TBank TaskRegistry::AllocateBank() noexcept
{
	const std::size_t bankIndex = bankCount.load(std::memory_order::relaxed);
	if (bankIndex >= MaxBanks)
	{
		ReportRefusedGrowth((bankIndex + 1) * recordsPerBank);

		return nullptr;
	}

	if (maxCapacityRecords > 0 && (bankIndex + 1) * recordsPerBank > maxCapacityRecords)
	{
		ReportRefusedGrowth((bankIndex + 1) * recordsPerBank);

		return nullptr;
	}

	const std::size_t bankBytes = recordsPerBank * sizeof(Record);
	auto records = static_cast<Record*>(MemoryManager::GetInstance().Allocate(bankAllocatorID, bankBytes));
	if (records == nullptr)
	{
		auto log = Logger::Get(name);
		log.OutError([name = name, recordsPerBank = recordsPerBank, bankBytes](auto& ls)
		{
			ls << name.c_str() << " could not take a bank of " << recordsPerBank << " task records, " << bankBytes
			   << " bytes. The table keeps the capacity it has. ";
		});

		return nullptr;
	}

	for (std::size_t record = 0; record < recordsPerBank; ++record)
	{
		new (&records[record]) Record();
	}

	banks[bankIndex] = records;
	bankCount.store(bankIndex + 1, std::memory_order::release);

	const std::size_t firstRecordOfBank = bankIndex * recordsPerBank;
	std::size_t nextFree = freeRecordHead;
	for (std::size_t offset = recordsPerBank; offset > 0; --offset)
	{
		Record& record = records[offset - 1];
		record.nextFreeRecord = nextFree;
		nextFree = firstRecordOfBank + offset - 1;
	}

	freeRecordHead = firstRecordOfBank;

	return records;
}

void TaskRegistry::DeallocateBank(TBank bank) noexcept
{
	if (bank == nullptr)
	{
		return;
	}

	for (std::size_t record = 0; record < recordsPerBank; ++record)
	{
		bank[record].~Record();
	}

	MemoryManager::GetInstance().Deallocate(bankAllocatorID, bank, recordsPerBank * sizeof(Record));
}

void TaskRegistry::ReportNoRoom(StaticString taskName) noexcept
{
	auto log = Logger::Get(name);
	log.OutError([name = name, taskName, capacity = GetCapacity(), maxCapacity = maxCapacityRecords](auto& ls)
	{
		ls << name.c_str() << " refused to track " << taskName.c_str() << " because all " << capacity
		   << " task records are in use. The task was not tracked and will not run; growing the registry is the"
		   << " fix, and its ceiling is " << (maxCapacity == 0 ? 0 : maxCapacity)
		   << " records, where 0 means no ceiling. ";
	});
}

void TaskRegistry::ReportRefusedGrowth(std::size_t requestedCapacity) noexcept
{
	auto log = Logger::Get(name);
	log.OutError([name = name, requestedCapacity, capacity = GetCapacity(), maxCapacity = maxCapacityRecords,
				  banks = MaxBanks](auto& ls)
	{
		ls << name.c_str() << " could not grow to " << requestedCapacity << " task records; it holds " << capacity
		   << ". The ceiling is " << (maxCapacity == 0 ? 0 : maxCapacity)
		   << " records, where 0 means no ceiling, and the bank table holds at most " << banks << " banks. ";
	});
}
} // namespace hbe

#ifdef TEST_ENABLED
#include "Engine/Engine.h"

namespace hbe
{
namespace
{
TRunnable RegistryTestRunnable() noexcept
{
	return [](void*, std::size_t, std::size_t) -> std::size_t { return 1; };
}
} // namespace

void TaskRegistryTest::Prepare()
{
	AddTest("The engine's registry is sized by the decided figures", [this](auto& ls)
	{
		auto& registry = Engine::Get().GetTaskSystem().GetRegistry();

		constexpr std::size_t decidedInitialRecords = 4096;
		constexpr std::size_t decidedGrowByRecords = 4096;
		constexpr std::size_t decidedMaxRecords = 0;

		static_assert(TaskRegistry::DefaultInitialCapacityRecords == decidedInitialRecords);
		static_assert(TaskRegistry::DefaultGrowByRecords == decidedGrowByRecords);
		static_assert(TaskRegistry::DefaultMaxCapacityRecords == decidedMaxRecords);

		ls << "Engine registry: capacity = " << registry.GetCapacity() << ", growing by " << registry.GetGrowBy()
		   << ", ceiling = " << registry.GetMaxCapacity() << ", record = " << TaskRegistry::RecordSizeBytes
		   << " bytes, so the table costs " << (registry.GetCapacity() * TaskRegistry::RecordSizeBytes) / 1024
		   << " KiB." << lf;

		if (registry.GetCapacity() != decidedInitialRecords)
		{
			ls << "The engine's registry holds " << registry.GetCapacity() << " task records, not "
			   << decidedInitialRecords << ", so the size R22 decided is not the size being built." << lferr;
		}

		if (registry.GetGrowBy() != decidedGrowByRecords)
		{
			ls << "The engine's registry grows by " << registry.GetGrowBy() << " records, not " << decidedGrowByRecords
			   << '.' << lferr;
		}

		if (registry.GetMaxCapacity() != decidedMaxRecords)
		{
			ls << "The engine's registry caps itself at " << registry.GetMaxCapacity()
			   << " records; R21's inert default is no ceiling." << lferr;
		}
	});

	AddTest("An identity resolves to its task until that task is released", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		const auto id = registry.Create("TrackedTask", RegistryTestRunnable(), nullptr);
		auto* found = registry.Find(id);

		ls << "Created record " << id.index << " generation " << id.generation << ", found = " << (found != nullptr)
		   << ", count = " << registry.GetCount() << '.' << lf;

		if (id.IsNull() || found == nullptr)
		{
			ls << "A task created in a table with room to spare did not resolve, so nothing can be dispatched"
			   << " through an identity." << lferr;

			return;
		}

		if (found->GetName() != StaticString("TrackedTask"))
		{
			ls << "The identity resolved to a task named something else, so records are not being read at the"
			   << " index the ID names." << lferr;
		}

		registry.Release(id);

		if (registry.Find(id) != nullptr)
		{
			ls << "A released task still resolved through its identity, so a work item could run against a task"
			   << " nobody owns." << lferr;
		}

		if (registry.GetCount() != 0)
		{
			ls << "Releasing did not return the record to the table; the count is " << registry.GetCount() << '.'
			   << lferr;
		}
	});

	AddTest("A recycled record is issued with a new generation", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		const auto first = registry.Create("FirstTask", RegistryTestRunnable(), nullptr);
		registry.Release(first);
		const auto second = registry.Create("SecondTask", RegistryTestRunnable(), nullptr);

		ls << "First identity: record " << first.index << " generation " << first.generation
		   << ". Second identity: record " << second.index << " generation " << second.generation << '.' << lf;

		if (second.IsNull())
		{
			ls << "A released record could not be issued again, so the table leaks records." << lferr;

			return;
		}

		if (second.index != first.index)
		{
			ls << "The freed record was not reused: it came back as record " << second.index << " instead of "
			   << first.index << ", so the free list is not being walked." << lferr;
		}

		if (second.generation == first.generation)
		{
			ls << "Record " << second.index << " was issued twice with generation " << second.generation
			   << ", so an ID naming the first task is indistinguishable from one naming the second - which is the"
			   << " whole of R7." << lferr;
		}

		if (registry.Find(first) != nullptr)
		{
			ls << "The first task's identity resolved to the second task, so a stale reference is followed instead"
			   << " of dropped." << lferr;
		}

		if (registry.Find(second) == nullptr)
		{
			ls << "The newly issued task does not resolve through the identity it was given." << lferr;
		}
	});

	AddTest("Growing adds exactly one bank and keeps earlier identities valid", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		const auto beforeGrowth = registry.Create("BeforeGrowth", RegistryTestRunnable(), nullptr);
		const auto capacityBefore = registry.GetCapacity();

		const bool grew = registry.Grow();
		const auto capacityAfter = registry.GetCapacity();

		constexpr std::size_t expectedCapacityAfter = 12;

		ls << "Capacity " << capacityBefore << " grown by " << registry.GetGrowBy() << " gave " << capacityAfter
		   << ", and the identity issued before the growth "
		   << (registry.Find(beforeGrowth) != nullptr ? "still" : "no longer") << " resolves." << lf;

		if (!grew)
		{
			ls << "Grow refused with no ceiling set, so growth is gated on something the caller cannot see." << lferr;
		}

		if (capacityBefore != 8)
		{
			ls << "A table built with 8 records in banks of 4 reports " << capacityBefore << '.' << lferr;
		}

		if (capacityAfter != expectedCapacityAfter)
		{
			ls << "Growing a 8-record table by 4 records gave " << capacityAfter << ", not " << expectedCapacityAfter
			   << ", so growth is not the exact bank the design states." << lferr;
		}

		if (registry.Find(beforeGrowth) == nullptr)
		{
			ls << "Growing the table invalidated an identity that was live before it, so work already queued would"
			   << " be dropped." << lferr;
		}

		const auto afterGrowth = registry.Create("AfterGrowth", RegistryTestRunnable(), nullptr);
		if (registry.Find(afterGrowth) == nullptr)
		{
			ls << "A task created in the bank that growth added does not resolve." << lferr;
		}
	});

	AddTest("A ceiling refuses the growth that would cross it", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		constexpr std::size_t ceiling = 12;
		registry.SetMaxCapacity(ceiling);

		const bool firstGrowth = registry.Grow();
		const bool secondGrowth = registry.Grow();

		ls << "Ceiling " << ceiling << ": growth to " << registry.GetCapacity() << " records "
		   << (firstGrowth ? "allowed" : "refused") << ", the next growth " << (secondGrowth ? "allowed" : "refused")
		   << '.' << lf;

		if (!firstGrowth)
		{
			ls << "A growth landing exactly on the ceiling was refused, so the ceiling excludes the size it is set"
			   << " to instead of the sizes above it." << lferr;
		}

		if (secondGrowth)
		{
			ls << "The table grew to " << registry.GetCapacity() << " records past a ceiling of " << ceiling
			   << ", so the cap is decorative." << lferr;
		}

		if (registry.GetCapacity() != ceiling)
		{
			ls << "Capacity is " << registry.GetCapacity() << " after growth against a ceiling of " << ceiling << '.'
			   << lferr;
		}
	});

	AddTest("Creation is refused when the table is full, and it does not grow to save the task", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 4, 4);

		TaskID ids[4];
		for (std::size_t index = 0; index < 4; ++index)
		{
			ids[index] = registry.Create("FillingTask", RegistryTestRunnable(), nullptr);
		}

		const auto overflow = registry.Create("OverflowTask", RegistryTestRunnable(), nullptr);
		const auto capacity = registry.GetCapacity();
		const auto count = registry.GetCount();

		ls << "A 4-record table: four creations " << (ids[3].IsNull() ? "failed" : "succeeded") << ", the fifth "
		   << (overflow.IsNull() ? "was refused" : "was admitted") << ", capacity " << capacity << ", count " << count
		   << '.' << lf;

		if (!overflow.IsNull())
		{
			ls << "A fifth task was tracked by a table holding 4 records, so records are being handed out that the"
			   << " table cannot hold." << lferr;
		}

		if (capacity != 4)
		{
			ls << "Creation grew the table to " << capacity << " records. Growing is a decision for whoever owns"
			   << " the registry, not something the dispatch path pays for." << lferr;
		}

		if (count != 4)
		{
			ls << "The count is " << count << " with four tasks tracked and one refused." << lferr;
		}

		registry.Release(ids[2]);
		const auto afterRelease = registry.Create("ReuseTask", RegistryTestRunnable(), nullptr);
		if (afterRelease.IsNull())
		{
			ls << "A record released from a full table could not be reissued, so the free list breaks once the"
			   << " table has been filled." << lferr;
		}
	});

	AddTest("A double release does not hand one record to two tasks", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 4, 4);

		const auto id = registry.Create("OnceOnly", RegistryTestRunnable(), nullptr);
		registry.Release(id);
		registry.Release(id);

		const auto count = registry.GetCount();
		const auto other = registry.Create("OtherTask", RegistryTestRunnable(), nullptr);

		ls << "After a double release the count is " << count << ", and the next creation returned record "
		   << other.index << " generation " << other.generation << " of the record " << id.index << '.' << lf;

		if (count != 0)
		{
			ls << "A double release left the count at " << count << ", so a record is counted twice as used or as"
			   << " free." << lferr;
		}

		if (other.index != id.index)
		{
			ls << "The record released twice came back as " << other.index << " rather than " << id.index
			   << ", which means the free list now holds it twice or lost it." << lferr;
		}

		std::size_t furtherAdmitted = 0;
		for (std::size_t index = 0; index < 8; ++index)
		{
			if (registry.Create("FillerTask", RegistryTestRunnable(), nullptr).IsNull())
			{
				break;
			}

			++furtherAdmitted;
		}

		ls << "After the double release the table admitted " << furtherAdmitted << " further tasks." << lf;

		if (furtherAdmitted != 3)
		{
			ls << "A 4-record table with one record already held admitted " << furtherAdmitted
			   << " further tasks, not 3, so the free list holds a record twice or has lost one." << lferr;
		}

		if (registry.GetCount() != 4)
		{
			ls << "The table reports " << registry.GetCount() << " records in use while holding four tasks." << lferr;
		}
	});

	AddTest("A successor is recorded on the task and read back by identity", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		ls << "A record costs " << TaskRegistry::RecordSizeBytes << " bytes, which is "
		   << TaskRegistry::RecordSizeBytes / 64 << " cache lines, so the default table is "
		   << TaskRegistry::DefaultInitialCapacityRecords * TaskRegistry::RecordSizeBytes / 1048576 << " MiB." << lf;

		const auto first = registry.Create("FirstTask", RegistryTestRunnable(), nullptr);
		const auto second = registry.Create("SecondTask", RegistryTestRunnable(), nullptr);

		registry.SetSuccessor(first, second);

		if (registry.GetSuccessor(first) != second)
		{
			ls << "A successor recorded for a tracked task did not come back, so the routing the pass acts on is"
			   << " not being stored." << lferr;
		}

		if (!registry.GetSuccessor(second).IsNull())
		{
			ls << "Recording a successor for one task also gave another task a successor, so the field is not"
			   << " per record." << lferr;
		}

		registry.Release(first);

		if (!registry.GetSuccessor(first).IsNull())
		{
			ls << "A released task still reported a successor. Nothing can dispatch on a task that no longer"
			   << " exists, so the answer must be nobody." << lferr;
		}
	});

	AddTest("A recycled record does not inherit the previous occupant's successor", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		const auto first = registry.Create("FirstTask", RegistryTestRunnable(), nullptr);
		const auto second = registry.Create("SecondTask", RegistryTestRunnable(), nullptr);
		registry.SetSuccessor(first, second);
		registry.Release(first);

		const auto recycled = registry.Create("RecycledTask", RegistryTestRunnable(), nullptr);
		if (recycled.index != first.index)
		{
			ls << "The test expected the freed record to be reused so it could check what it inherits; it came"
			   << " back as record " << recycled.index << " instead of " << first.index << ", so this proves nothing."
			   << lf;
		}

		if (!registry.GetSuccessor(recycled).IsNull())
		{
			ls << "Record " << recycled.index << " was issued to a new task still holding the successor of the"
			   << " task that had it before. A dispatch that never happened would run on this task's join." << lferr;
		}
	});

	AddTest("A successor cannot be recorded through an identity the registry does not honour", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("RegistryTest", 8, 4);

		const auto live = registry.Create("LiveTask", RegistryTestRunnable(), nullptr);
		const auto other = registry.Create("OtherTask", RegistryTestRunnable(), nullptr);

		const TaskID staleIdentity{live.index, live.generation + 7};

		registry.SetSuccessor(staleIdentity, other);

		if (!registry.GetSuccessor(live).IsNull())
		{
			ls << "An ID at generation " << staleIdentity.generation << " recorded a successor on record " << live.index
			   << ", which generation " << live.generation << " is still using. That is the alias"
			   << " R7 closes for lookups, reaching the routing field." << lferr;
		}

		registry.SetSuccessor({}, other);

		if (!registry.GetSuccessor(live).IsNull() || !registry.GetSuccessor(other).IsNull())
		{
			ls << "A null task identity routed a successor onto a live task." << lferr;
		}

		registry.SetSuccessor(other, {});

		if (!registry.GetSuccessor(other).IsNull())
		{
			ls << "Clearing a successor by naming no task left a routing decision in place." << lferr;
		}
	});

	AddTest("The work item that closes a join is the last reserved one, and only that one", [this](auto& ls)
	{
		Task task("JoinTask", RegistryTestRunnable(), nullptr);

		if (task.HasDone())
		{
			ls << "A task with nothing reserved reported itself done before anything ran." << lferr;
		}

		task.ReserveSubTasks(3);

		const bool first = task.ReportFinishedSubTask();
		const bool second = task.ReportFinishedSubTask();

		if (first || second || task.HasDone())
		{
			ls << "A join of three closed after two items, so whatever waits on it is woken early and reads"
			   << " results that were never written." << lferr;
		}

		const bool third = task.ReportFinishedSubTask();
		const bool fourth = task.ReportFinishedSubTask();

		if (!third)
		{
			ls << "The last reserved item did not report the join closed, so no successor would ever be"
			   << " dispatched." << lferr;
		}

		if (!task.HasDone())
		{
			ls << "All three reserved items finished and the task still reports it is not done." << lferr;
		}

		if (fourth)
		{
			ls << "A finish beyond the reservation also reported the join closed. Two winners means the"
			   << " successor is dispatched twice." << lferr;
		}
	});

	AddTest("A work item carries the identity of the task that generated it", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		const auto id = taskSystem.CreateTask("IdentityCarrier", [](void*, std::size_t, std::size_t) -> std::size_t
		{ return 1; }, nullptr);

		auto* task = taskSystem.FindTask(id);
		if (task == nullptr)
		{
			ls << "The task system could not track a task, so identity cannot be checked at all." << lferr;

			return;
		}

		task->ReserveSubTasks(1);
		const auto subtask = task->GenerateSubTask(0, 1, 0);

		ls << "Task identity: record " << id.index << " generation " << id.generation << "; subtask carries record "
		   << subtask.taskID.index << " generation " << subtask.taskID.generation << '.' << lf;

		if (task->GetID() != id)
		{
			ls << "The task does not carry the identity it was issued, so a subtask generated from it cannot name"
			   << " it." << lferr;
		}

		if (subtask.taskID != id)
		{
			ls << "The subtask carries a different identity from its task, which is what the generation check is"
			   << " supposed to reject - here it would reject live work." << lferr;
		}

		taskSystem.ReleaseTask(id);
	});
}
} // namespace hbe
#endif //TEST_ENABLED
