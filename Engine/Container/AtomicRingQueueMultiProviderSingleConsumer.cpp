// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AtomicRingQueueMultiProviderSingleConsumer.h"

#ifdef __UNIT_TEST__
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace hbe
{
namespace
{
struct StallingValue
{
	int serial = -1;
	std::atomic<bool>* arrived = nullptr;
	std::atomic<bool>* unblock = nullptr;

	StallingValue() = default;

	StallingValue(int inSerial, std::atomic<bool>* inArrived = nullptr, std::atomic<bool>* inUnblock = nullptr)
		: serial(inSerial)
		, arrived(inArrived)
		, unblock(inUnblock)
	{
	}

	StallingValue(const StallingValue& inOther)
		: serial(inOther.serial)
		, arrived(inOther.arrived)
		, unblock(inOther.unblock)
	{
		if (arrived != nullptr)
		{
			arrived->store(true, std::memory_order_release);
		}

		if (unblock != nullptr)
		{
			while (!unblock->load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}
		}
	}

	StallingValue& operator=(const StallingValue&) = default;
	StallingValue& operator=(StallingValue&&) noexcept = default;
	~StallingValue() = default;
};

void WaitForFlag(const std::atomic<bool>& inFlag)
{
	while (!inFlag.load(std::memory_order_acquire))
	{
		std::this_thread::yield();
	}
}
} // namespace

void AtomicRingQueueMultiProviderSingleConsumerTest::Prepare()
{
	AddTest("Push and Pop Preserve FIFO Order", [this](auto& ls)
	{
		using Queue = AtomicRingQueueMPSC<int, 4>;

		Queue queue(16, 4);
		if (!queue.IsEmpty())
		{
			ls << "A fresh queue reported work it never received." << lferr;

			return;
		}

		constexpr int count = 30;
		for (int value = 0; value < count; ++value)
		{
			if (queue.Push(value) != Queue::EPushResult::Pushed)
			{
				ls << "Push " << value << " was refused although the fuse holds 64 slots." << lferr;

				return;
			}
		}

		if (queue.IsEmpty())
		{
			ls << "Thirty values were pushed and the queue still calls itself empty." << lferr;

			return;
		}

		int expected = 0;
		int value = -1;
		auto result = queue.Pop(value);
		for (; result == Queue::EPopResult::Element; result = queue.Pop(value))
		{
			if (value != expected)
			{
				ls << "Position " << expected << " came out as " << value << ", so a slot came out of order." << lferr;

				return;
			}

			++expected;
			value = -1;
		}

		if (expected != count || result != Queue::EPopResult::Empty || !queue.IsEmpty())
		{
			ls << "Draining returned " << expected << " value(s) of " << count << " and ended on result "
			   << static_cast<int>(result) << "." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("Four Producers and One Consumer Lose Nothing", [this](auto& ls)
	{
		using Queue = AtomicRingQueueMPSC<uint32_t, 8>;

		constexpr size_t producerCount = 4;
		constexpr size_t perProducer = 4000;
		constexpr size_t total = producerCount * perProducer;

		Queue queue(4096, producerCount + 1);

		std::atomic<int> refusals{0};
		std::vector<uint32_t> collected;
		collected.reserve(total);

		std::array<std::thread, producerCount> threads;
		for (size_t slot = 0; slot < producerCount; ++slot)
		{
			threads[slot] = std::thread([&queue, &refusals](size_t producerSlot)
			{
				for (size_t index = 0; index < perProducer; ++index)
				{
					const uint32_t value = static_cast<uint32_t>(producerSlot * perProducer + index);
					if (queue.Push(value) != Queue::EPushResult::Pushed)
					{
						refusals.fetch_add(1, std::memory_order_relaxed);

						return;
					}
				}
			}, slot);
		}

		std::thread consumer([&queue, &collected]()
		{
			uint32_t value = 0;
			while (collected.size() < total)
			{
				if (queue.Pop(value) == Queue::EPopResult::Element)
				{
					collected.push_back(value);
				}
				else
				{
					std::this_thread::yield();
				}
			}
		});

		for (auto& thread : threads)
		{
			thread.join();
		}

		consumer.join();

		if (refusals.load(std::memory_order_relaxed) != 0)
		{
			ls << "A producer was refused " << refusals.load(std::memory_order_relaxed)
			   << " time(s) even though the fuse holds far more than the whole batch." << lferr;

			return;
		}

		if (collected.size() != total)
		{
			ls << "The consumer collected " << static_cast<int>(collected.size()) << " value(s) of "
			   << static_cast<int>(total) << "." << lferr;

			return;
		}

		std::sort(collected.begin(), collected.end());
		for (size_t index = 0; index < total; ++index)
		{
			if (collected[index] != index)
			{
				ls << "Sorted value " << static_cast<int>(index) << " came out as " << collected[index]
				   << ", so a claimed slot was either skipped or handed out twice." << lferr;

				return;
			}
		}

		if (queue.GetGrowCount() <= 1)
		{
			ls << "Sixteen thousand values moved through blocks of 8 slots without ever growing past one block."
			   << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("Block Boundaries Grow the Chain and Recycle It", [this](auto& ls)
	{
		using Queue = AtomicRingQueueMPSC<uint32_t, 4>;

		constexpr size_t total = 4000;
		constexpr size_t maxBlocks = 4;

		Queue queue(maxBlocks, 2);
		std::vector<uint32_t> collected;
		collected.reserve(total);

		std::thread producer([&queue]()
		{
			for (uint32_t value = 0; value < total; ++value)
			{
				while (queue.Push(value) == Queue::EPushResult::Full)
				{
					std::this_thread::yield();
				}
			}
		});

		uint32_t value = 0;
		while (collected.size() < total)
		{
			if (queue.Pop(value) == Queue::EPopResult::Element)
			{
				collected.push_back(value);
			}
			else
			{
				std::this_thread::yield();
			}
		}

		producer.join();

		if (collected.size() != total)
		{
			ls << "The consumer collected " << static_cast<int>(collected.size()) << " value(s) of "
			   << static_cast<int>(total) << "." << lferr;

			return;
		}

		for (size_t index = 0; index < total; ++index)
		{
			if (collected[index] != index)
			{
				ls << "FIFO order broke at " << static_cast<int>(index) << ": " << collected[index] << "." << lferr;

				return;
			}
		}

		if (queue.GetGrowCount() <= maxBlocks)
		{
			ls << "The chain grew " << static_cast<int>(queue.GetGrowCount()) << " block(s) for a fuse of " << maxBlocks
			   << ", so recycled memory was never handed out again." << lferr;

			return;
		}

		if (queue.GetRetireCount() == 0)
		{
			ls << "A drained block was never retired, so recycling did not happen." << lferr;

			return;
		}

		if (queue.GetGrowCount() - queue.GetRetireCount() > maxBlocks)
		{
			ls << "Live blocks passed the fuse: " << static_cast<int>(queue.GetGrowCount()) << " grown minus "
			   << static_cast<int>(queue.GetRetireCount()) << " retired against a cap of " << maxBlocks << "." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("A Claimed but Unpublished Slot Reads as Blocked, Never as Empty", [this](auto& ls)
	{
		using Queue = AtomicRingQueueMPSC<StallingValue, 2>;

		Queue queue(8, 4);
		std::atomic<bool> arrived{false};
		std::atomic<bool> unblock{false};

		std::thread parked([&queue, &arrived, &unblock]() { (void) queue.Push(StallingValue(0, &arrived, &unblock)); });

		WaitForFlag(arrived);

		for (int value = 1; value < 4; ++value)
		{
			(void) queue.Push(StallingValue(value));
		}

		StallingValue item;
		const auto blocked = queue.Pop(item);
		if (blocked != Queue::EPopResult::Blocked)
		{
			unblock.store(true, std::memory_order_release);
			parked.join();
			ls << "The consumer returned " << static_cast<int>(blocked)
			   << " for a slot a producer had claimed but not published; it must report Blocked." << lferr;

			return;
		}

		if (item.serial != -1)
		{
			unblock.store(true, std::memory_order_release);
			parked.join();
			ls << "A Blocked pop still wrote serial " << item.serial << " into the caller's slot." << lferr;

			return;
		}

		if (queue.IsEmpty())
		{
			unblock.store(true, std::memory_order_release);
			parked.join();
			ls << "A queue holding an in-flight claim reported itself empty." << lferr;

			return;
		}

		unblock.store(true, std::memory_order_release);
		parked.join();

		int expected = 0;
		for (auto result = queue.Pop(item); result != Queue::EPopResult::Empty; result = queue.Pop(item))
		{
			if (result != Queue::EPopResult::Element)
			{
				std::this_thread::yield();

				continue;
			}

			if (item.serial != expected)
			{
				ls << "After the stall, position " << expected << " came out as serial " << item.serial
				   << ", so the blocked slot was skipped rather than waited for." << lferr;

				return;
			}

			++expected;
			item.serial = -1;
		}

		if (expected != 4 || !queue.IsEmpty())
		{
			ls << "Draining after the stall returned " << expected << " value(s) of 4." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("A Producer Inside a Block Keeps That Block Alive", [this](auto& ls)
	{
		using Queue = AtomicRingQueueMPSC<StallingValue, 2>;

		constexpr size_t maxBlocks = 3;
		Queue queue(maxBlocks, 4);

		std::atomic<bool> arrived{false};
		std::atomic<bool> unblock{false};

		std::thread parked([&queue, &arrived, &unblock]() { (void) queue.Push(StallingValue(0, &arrived, &unblock)); });

		WaitForFlag(arrived);

		for (int value = 1; value < 4; ++value)
		{
			(void) queue.Push(StallingValue(value));
		}

		StallingValue item;
		int blockedCount = 0;
		for (int attempt = 0; attempt < 16; ++attempt)
		{
			if (queue.Pop(item) == Queue::EPopResult::Blocked)
			{
				++blockedCount;
			}
		}

		if (blockedCount == 0)
		{
			unblock.store(true, std::memory_order_release);
			parked.join();
			ls << "Every pop on a queue whose oldest slot was still unpublished came back as something other "
			   << "than Blocked." << lferr;

			return;
		}

		if (queue.GetRetireCount() != 0)
		{
			unblock.store(true, std::memory_order_release);
			parked.join();
			ls << "A block a producer was still inside got retired, so its memory went back while still in use."
			   << lferr;

			return;
		}

		unblock.store(true, std::memory_order_release);
		parked.join();

		int expected = 0;
		for (auto result = queue.Pop(item); result != Queue::EPopResult::Empty; result = queue.Pop(item))
		{
			if (result != Queue::EPopResult::Element)
			{
				std::this_thread::yield();

				continue;
			}

			if (item.serial != expected)
			{
				ls << "Position " << expected << " came out as serial " << item.serial << "." << lferr;

				return;
			}

			++expected;
			item.serial = -1;
		}

		if (expected != 4)
		{
			ls << "Draining returned " << expected << " value(s) of 4." << lferr;

			return;
		}

		for (int round = 0; round < 8; ++round)
		{
			for (int value = 0; value < 4; ++value)
			{
				while (queue.Push(StallingValue(value)) == Queue::EPushResult::Full)
				{
					std::this_thread::yield();
				}
			}

			for (int value = 0; value < 4; ++value)
			{
				while (queue.Pop(item) != Queue::EPopResult::Element)
				{
					std::this_thread::yield();
				}

				if (item.serial != value)
				{
					ls << "Recycled memory came back wrong in round " << round << ", slot " << value << ": serial "
					   << item.serial << "." << lferr;

					return;
				}

				item.serial = -1;
			}
		}

		if (queue.GetGrowCount() <= maxBlocks)
		{
			ls << "Eight rounds of four pushes never recreated a block past the fuse of " << maxBlocks
			   << ", so retired blocks were not reused." << lferr;

			return;
		}

		if (queue.GetRetireCount() == 0)
		{
			ls << "Nothing was retired even though the chain turned over eight times." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("The Block Fuse Stops Growth and Reports Full", [this](auto& ls)
	{
		using Queue = AtomicRingQueueMPSC<int, 2>;

		constexpr size_t maxBlocks = 3;
		constexpr int capacity = static_cast<int>(maxBlocks * 2);

		Queue queue(maxBlocks, 2);
		int accepted = 0;
		for (int value = 0; value < capacity * 4; ++value)
		{
			breakIf(queue.Push(value) != Queue::EPushResult::Pushed);

			++accepted;
		}

		if (accepted != capacity)
		{
			ls << "A queue with a fuse of " << maxBlocks << " blocks of 2 slots accepted " << accepted
			   << " push(es) before reporting Full." << lferr;

			return;
		}

		if (queue.GetGrowCount() != maxBlocks)
		{
			ls << "The chain grew " << static_cast<int>(queue.GetGrowCount()) << " block(s) against a fuse of "
			   << maxBlocks << "." << lferr;

			return;
		}

		int expected = 0;
		int value = -1;
		for (auto result = queue.Pop(value); result != Queue::EPopResult::Empty; result = queue.Pop(value))
		{
			if (result != Queue::EPopResult::Element)
			{
				std::this_thread::yield();

				continue;
			}

			if (value != expected)
			{
				ls << "A refused push disturbed FIFO order: expected " << expected << ", got " << value << "." << lferr;

				return;
			}

			++expected;
			value = -1;
		}

		if (expected != capacity)
		{
			ls << "Draining the fused queue returned " << expected << " value(s) of " << capacity << "." << lferr;

			return;
		}

		if (queue.Push(capacity) != Queue::EPushResult::Pushed)
		{
			ls << "After a full drain the fuse still counted the retired blocks, so the queue could not accept "
				  "again."
			   << lferr;

			return;
		}

		(void) queue.Pop(value);
		ls << "Pass" << lf;
	});
}
} // namespace hbe
#endif // __UNIT_TEST__
