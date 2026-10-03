// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "ResultPacket.h"

#include <cstring>

#include "Task.h"
#include "TaskRegistry.h"
#include "WorkItem.h"

#ifdef __UNIT_TEST__
namespace hbe
{
namespace
{
TRunnable PacketTestRunnable() noexcept
{
	return [](void*, std::size_t, std::size_t) -> std::size_t { return 1; };
}
} // namespace

void ResultPacketTest::Prepare()
{
	AddTest("One packet prices a task and a record at the figures that were decided", [this](auto& ls)
	{
		constexpr std::size_t decidedPacketBytes = 128;
		constexpr std::size_t decidedTaskBytes = 200;
		constexpr std::size_t decidedRecordBytes = 256;
		constexpr std::size_t decidedTableKib = 1024;
		constexpr std::size_t decidedWorkItemBytes = 80;

		const auto tableBytes = TaskRegistry::DefaultInitialCapacityRecords * TaskRegistry::RecordSizeBytes;

		ls << "Packet " << sizeof(ResultPacket) << " bytes, task " << sizeof(Task) << " bytes, record "
		   << TaskRegistry::RecordSizeBytes << " bytes (" << TaskRegistry::RecordSizeBytes / 64
		   << " cache lines), a queued work item " << sizeof(WorkItem) << " bytes, and the default table is "
		   << tableBytes / 1024 << " KiB." << lf;

		if (sizeof(WorkItem) != decidedWorkItemBytes)
		{
			ls << "A queued work item is " << sizeof(WorkItem) << " bytes, not " << decidedWorkItemBytes
			   << ". An item is copied on every enqueue, every re-add after a partial run and every sweep that moves"
			   << " it between lanes, so this width is paid per lane change rather than per task - which is why"
			   << " RangedTask's name copy came out when the item type was slimmed." << lferr;
		}

		if (sizeof(ResultPacket) != decidedPacketBytes)
		{
			ls << "A result packet is " << sizeof(ResultPacket) << " bytes, not " << decidedPacketBytes
			   << ". The task is not a handle to a buffer any more, so this width is the cost of the design rather"
			   << " than an implementation detail." << lferr;
		}

		if (sizeof(Task) != decidedTaskBytes)
		{
			ls << "A task is " << sizeof(Task) << " bytes, not " << decidedTaskBytes << ", so the record no longer"
			   << " costs what the decision priced it at." << lferr;
		}

		if (TaskRegistry::RecordSizeBytes != decidedRecordBytes)
		{
			ls << "A record is " << TaskRegistry::RecordSizeBytes << " bytes, not " << decidedRecordBytes
			   << ". Every byte added here is multiplied by the registry's capacity." << lferr;
		}

		if (tableBytes / 1024 != decidedTableKib)
		{
			ls << "The default identity table costs " << tableBytes / 1024 << " KiB rather than " << decidedTableKib
			   << " KiB." << lferr;
		}

		if (TaskRegistry::RecordSizeBytes % 64 != 0)
		{
			ls << "A record of " << TaskRegistry::RecordSizeBytes
			   << " bytes is not a whole number of cache lines, so neighbouring records share lines - which is the"
			   << " reason padding the packet to a line boundary was rejected." << lferr;
		}
	});

	AddTest("A record the registry issues has no result, whatever it held before", [this](auto& ls)
	{
		TaskRegistry registry;
		registry.Initialize("PacketTest", 8, 4);

		const auto firstID = registry.Create("PacketFirstOccupant", PacketTestRunnable(), nullptr);
		auto* firstTask = registry.Find(firstID);
		if (firstTask == nullptr)
		{
			ls << "A registry with room did not issue a task, so nothing here can be checked." << lferr;

			return;
		}

		auto& packet = firstTask->GetResult();

		ls << "Issued record " << firstID.index << " generation " << firstID.generation << ": kind "
		   << static_cast<int>(packet.GetKind()) << ", destination stream "
		   << static_cast<int>(packet.GetDestinationStreamIndex()) << ", HasResult = " << packet.HasResult() << '.'
		   << lf;

		if (packet.HasResult())
		{
			ls << "A task that has run nothing already reports a result of kind " << static_cast<int>(packet.GetKind())
			   << ", so a reader would act on the previous occupant of this"
			   << " record." << lferr;
		}

		if (packet.GetKind() != ResultPacket::KindNoResult)
		{
			ls << "A new record's packet reports kind " << static_cast<int>(packet.GetKind()) << " where"
			   << " KindNoResult was expected." << lferr;
		}

		if (packet.GetDestinationStreamIndex() != ResultPacket::NoDestinationStream)
		{
			ls << "A new record's packet is addressed to stream "
			   << static_cast<int>(packet.GetDestinationStreamIndex())
			   << ". Addressed-to is a decision the producer makes, not a default it inherits." << lferr;
		}

		std::memset(packet.GetPayload(), 0xA5, ResultPacket::PayloadBytes);
		packet.SetDestinationStreamIndex(3);
		packet.SetKind(ResultPacket::FirstApplicationKind + 6);

		ls << "Written by the producer: kind " << static_cast<int>(packet.GetKind()) << ", destination stream "
		   << static_cast<int>(packet.GetDestinationStreamIndex()) << ", payload ends "
		   << static_cast<int>(packet.GetPayload()[0]) << " and "
		   << static_cast<int>(packet.GetPayload()[ResultPacket::PayloadBytes - 1]) << ", payload alignment remainder "
		   << reinterpret_cast<std::uintptr_t>(packet.GetPayload()) % sizeof(std::uint64_t) << '.' << lf;

		if (!packet.HasResult())
		{
			ls << "A kind was set and the packet still says it holds no result, so the write and the read disagree"
			   << " about where the kind lives." << lferr;
		}

		if (packet.GetPayload()[0] != 0xA5 || packet.GetPayload()[ResultPacket::PayloadBytes - 1] != 0xA5)
		{
			ls << "Writing the payload did not reach both ends of it: first byte "
			   << static_cast<int>(packet.GetPayload()[0]) << ", last byte "
			   << static_cast<int>(packet.GetPayload()[ResultPacket::PayloadBytes - 1]) << '.' << lferr;
		}

		if (reinterpret_cast<std::uintptr_t>(packet.GetPayload()) % sizeof(std::uint64_t) != 0)
		{
			ls << "The payload is not reachable in aligned words, so a producer reading it as integers needs the"
			   << " unaligned access this packet was meant to rule out." << lferr;
		}

		registry.Release(firstID);

		const auto reusedID = registry.Create("PacketSecondOccupant", PacketTestRunnable(), nullptr);
		auto* reusedTask = registry.Find(reusedID);
		if (reusedTask == nullptr)
		{
			ls << "The registry could not issue the released record again, so the reuse case cannot be reached."
			   << lferr;

			return;
		}

		const auto& reusedPacket = reusedTask->GetResult();

		ls << "Reissued record " << reusedID.index << " generation " << reusedID.generation << ": kind "
		   << static_cast<int>(reusedPacket.GetKind()) << ", destination stream "
		   << static_cast<int>(reusedPacket.GetDestinationStreamIndex()) << ", payload byte 0 as "
		   << static_cast<int>(reusedPacket.GetPayload()[0]) << '.' << lf;

		if (reusedPacket.HasResult())
		{
			ls << "Record " << reusedID.index << " was issued to a new task while still reporting the previous"
			   << " task's result of kind " << static_cast<int>(reusedPacket.GetKind())
			   << ". The generation check stops a stale reader, and nothing stops this." << lferr;
		}

		if (reusedPacket.GetPayload()[0] == 0xA5)
		{
			ls << "Record " << reusedID.index << " still holds the bytes the previous task wrote." << lferr;
		}
	});

	AddTest("Clearing the kind is how a producer says there is no result", [this](auto& ls)
	{
		ResultPacket packet;
		packet.Clear();
		std::memset(packet.GetPayload(), 0x5A, ResultPacket::PayloadBytes);
		packet.SetKind(ResultPacket::KindNoResult);

		ls << "Payload filled with 0x5A and kind set to " << static_cast<int>(packet.GetKind())
		   << ": HasResult = " << packet.HasResult() << '.' << lf;

		if (packet.HasResult())
		{
			ls << "A packet whose kind is KindNoResult reports a result while holding bytes that are not one, so a"
			   << " reader would be handed whatever the last producer left behind." << lferr;
		}
	});
}
} // namespace hbe
#endif //__UNIT_TEST__
