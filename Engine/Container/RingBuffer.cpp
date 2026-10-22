// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RingBuffer.h"

#ifdef TEST_ENABLED
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "Memory/SystemAllocator.h"

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

template <typename T, size_t Capacity>
using TSysQueue = RingBuffer<T, AtomicFixedRingMPSC<T, Capacity>, SystemAllocator<T>>;

void RingBufferTest::Prepare()
{
	AddTest("Push and Pop Preserve FIFO Order", [this](auto& ls)
	{
		using Queue = TSysQueue<int, 4>;

		Queue queue(16, 4);
		if (!queue.IsEmpty())
		{
			ls << "A fresh queue reported work it never received." << lferr;

			return;
		}

		constexpr int count = 30;
		for (int value = 0; value < count; ++value)
		{
			if (queue.Push(value) != EPushResult::Pushed)
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
		for (; result == EPopResult::Element; result = queue.Pop(value))
		{
			if (value != expected)
			{
				ls << "Position " << expected << " came out as " << value << ", so a slot came out of order." << lferr;

				return;
			}

			++expected;
			value = -1;
		}

		if (expected != count || result != EPopResult::Empty || !queue.IsEmpty())
		{
			ls << "Draining returned " << expected << " value(s) of " << count << " and ended on result "
			   << static_cast<int>(result) << "." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("Four Producers and One Consumer Lose Nothing", [this](auto& ls)
	{
		using Queue = TSysQueue<uint32_t, 8>;

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
					if (queue.Push(value) != EPushResult::Pushed)
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
				if (queue.Pop(value) == EPopResult::Element)
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
		using Queue = TSysQueue<uint32_t, 4>;

		constexpr size_t total = 4000;
		constexpr size_t maxBlocks = 4;

		Queue queue(maxBlocks, 2);
		std::vector<uint32_t> collected;
		collected.reserve(total);

		std::thread producer([&queue]()
		{
			for (uint32_t value = 0; value < total; ++value)
			{
				while (queue.Push(value) == EPushResult::Full)
				{
					std::this_thread::yield();
				}
			}
		});

		uint32_t value = 0;
		while (collected.size() < total)
		{
			if (queue.Pop(value) == EPopResult::Element)
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
		using Queue = TSysQueue<StallingValue, 2>;

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
		if (blocked != EPopResult::Blocked)
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
		for (auto result = queue.Pop(item); result != EPopResult::Empty; result = queue.Pop(item))
		{
			if (result != EPopResult::Element)
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
		using Queue = TSysQueue<StallingValue, 2>;

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
			if (queue.Pop(item) == EPopResult::Blocked)
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
		for (auto result = queue.Pop(item); result != EPopResult::Empty; result = queue.Pop(item))
		{
			if (result != EPopResult::Element)
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
				while (queue.Push(StallingValue(value)) == EPushResult::Full)
				{
					std::this_thread::yield();
				}
			}

			for (int value = 0; value < 4; ++value)
			{
				while (queue.Pop(item) != EPopResult::Element)
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
		using Queue = TSysQueue<int, 2>;

		constexpr size_t maxBlocks = 3;
		constexpr int capacity = static_cast<int>(maxBlocks * 2);

		Queue queue(maxBlocks, 2);
		int accepted = 0;
		for (int value = 0; value < capacity * 4; ++value)
		{
			breakIf(queue.Push(value) != EPushResult::Pushed);

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
		for (auto result = queue.Pop(value); result != EPopResult::Empty; result = queue.Pop(value))
		{
			if (result != EPopResult::Element)
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

		if (queue.Push(capacity) != EPushResult::Pushed)
		{
			ls << "After a full drain the fuse still counted the retired blocks, so the queue could not accept "
				  "again."
			   << lferr;

			return;
		}

		(void) queue.Pop(value);
		ls << "Pass" << lf;
	});

	AddTest("Each Block Is Handed Over Exactly One Lap", [this](auto& ls)
	{
		using Queue = TSysQueue<uint32_t, 4>;

		constexpr size_t slots = 4;
		constexpr size_t laps = 3;

		Queue queue(laps + 1, 2);

		for (uint32_t value = 1; value <= slots * laps; ++value)
		{
			if (queue.Push(value) != EPushResult::Pushed)
			{
				ls << "Value " << value << " was refused although every block was allowed its own lap." << lferr;

				return;
			}
		}

		if (queue.GetGrowCount() != laps)
		{
			ls << "Twelve values entered blocks of four but the chain grew " << queue.GetGrowCount()
			   << " block(s) instead of three, so a block was reused before it was handed over." << lferr;

			return;
		}

		if (queue.GetRetireCount() != 0)
		{
			ls << "A block was retired with nobody consuming, so the consumer's cursor cannot be the ring's." << lferr;

			return;
		}

		uint32_t expected = 1;
		uint32_t value = 0;
		for (auto result = queue.Pop(value); result != EPopResult::Empty; result = queue.Pop(value))
		{
			if (result != EPopResult::Element)
			{
				ls << "A fully published chain answered " << static_cast<int>(result)
				   << " instead of handing over element " << expected << "." << lferr;

				return;
			}

			if (value != expected)
			{
				ls << "Element " << expected << " came out as " << value
				   << ", so a recycled slot overwrote what had not been delivered." << lferr;

				return;
			}

			++expected;
			value = 0;
		}

		if (expected != slots * laps + 1)
		{
			ls << "Draining three spent blocks delivered " << expected - 1 << " value(s) of " << slots * laps << "."
			   << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("A Sparse Producer Never Leaves The Consumer Blocked", [this](auto& ls)
	{
		using Queue = TSysQueue<uint32_t, 64>;

		constexpr size_t producerCount = 4;
		constexpr size_t perProducer = 10000;
		constexpr size_t total = producerCount * perProducer;

		Queue queue(64, producerCount + 1);

		std::atomic<size_t> delivered{0};
		std::atomic<size_t> blockedAnswers{0};
		std::atomic<size_t> corrupt{0};
		std::vector<std::atomic<unsigned char>> seen(total);
		std::atomic<bool> stalled{false};
		std::atomic<bool> start{false};

		std::array<std::thread, producerCount> threads;
		for (size_t slot = 0; slot < producerCount; ++slot)
		{
			threads[slot] = std::thread([&queue, &start](size_t producerSlot)
			{
				while (!start.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				for (size_t index = 0; index < perProducer; ++index)
				{
					while (queue.Push(static_cast<uint32_t>(producerSlot * perProducer + index)) != EPushResult::Pushed)
					{
						std::this_thread::yield();
					}

					std::this_thread::sleep_for(std::chrono::microseconds(2));
				}
			}, slot);
		}

		std::thread consumer([&queue, &delivered, &blockedAnswers, &corrupt, &seen, &stalled, &start]()
		{
			while (!start.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(15000);
			uint32_t value = 0;

			while (delivered.load(std::memory_order_relaxed) < total)
			{
				const EPopResult result = queue.Pop(value);
				if (result == EPopResult::Element)
				{
					if (value >= total || seen[value].exchange(1, std::memory_order_relaxed) != 0)
					{
						corrupt.fetch_add(1, std::memory_order_relaxed);
					}

					delivered.fetch_add(1, std::memory_order_relaxed);

					continue;
				}

				if (result == EPopResult::Blocked)
				{
					blockedAnswers.fetch_add(1, std::memory_order_relaxed);
				}

				if (std::chrono::steady_clock::now() > deadline)
				{
					stalled.store(true, std::memory_order_relaxed);

					return;
				}
			}
		});

		start.store(true, std::memory_order_release);

		for (auto& thread : threads)
		{
			thread.join();
		}

		consumer.join();

		if (stalled.load(std::memory_order_relaxed))
		{
			ls << "The queue delivered " << delivered.load(std::memory_order_relaxed) << " value(s) of " << total
			   << " and then went quiet for the whole budget, having returned Blocked "
			   << blockedAnswers.load(std::memory_order_relaxed)
			   << " time(s): a block kept a chain reservation its ring never learned about." << lferr;

			return;
		}

		const size_t damaged = corrupt.load(std::memory_order_relaxed);
		if (damaged != 0)
		{
			ls << damaged << " value(s) came out twice or outside the range the producers could send, out of "
			   << delivered.load(std::memory_order_relaxed) << " delivered." << lferr;

			return;
		}

		if (delivered.load(std::memory_order_relaxed) != total)
		{
			ls << "Delivered " << delivered.load(std::memory_order_relaxed) << " value(s) of " << total << "." << lferr;

			return;
		}

		ls << "Pass, Blocked answered " << blockedAnswers.load(std::memory_order_relaxed) << " time(s)." << lf;
	});

	AddTest("A Sparse Chain Under A Tight Fuse Never Jams", [this](auto& ls)
	{
		using Queue = TSysQueue<uint32_t, 64>;

		constexpr size_t producerCount = 4;
		constexpr size_t perProducer = 2000;
		constexpr size_t total = producerCount * perProducer;
		constexpr size_t rounds = 6;
		constexpr size_t maxBlocks = 6;

		size_t refused = 0;
		size_t blocked = 0;

		for (size_t round = 0; round < rounds; ++round)
		{
			Queue queue(maxBlocks, producerCount + 1);

			std::atomic<size_t> delivered{0};
			std::atomic<size_t> damaged{0};
			std::atomic<size_t> refusals{0};
			std::atomic<size_t> blockedAnswers{0};
			std::atomic<bool> stalled{false};
			std::atomic<bool> start{false};
			std::vector<std::atomic<unsigned char>> seen(total);

			for (std::atomic<unsigned char>& mark : seen)
			{
				mark.store(0, std::memory_order_relaxed);
			}

			std::array<std::thread, producerCount> threads;
			for (size_t slot = 0; slot < producerCount; ++slot)
			{
				threads[slot] = std::thread([&queue, &refusals, &start](size_t producerSlot)
				{
					while (!start.load(std::memory_order_acquire))
					{
						std::this_thread::yield();
					}

					for (size_t index = 0; index < perProducer; ++index)
					{
						while (queue.Push(static_cast<uint32_t>(producerSlot * perProducer + index)) !=
							   EPushResult::Pushed)
						{
							refusals.fetch_add(1, std::memory_order_relaxed);

							if (refusals.load(std::memory_order_relaxed) > total * 4)
							{
								return;
							}

							std::this_thread::yield();
						}

						std::this_thread::sleep_for(std::chrono::microseconds(2));
					}
				}, slot);
			}

			std::thread consumer([&queue, &delivered, &damaged, &blockedAnswers, &stalled, &start, &seen]()
			{
				while (!start.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(8000);
				uint32_t value = 0;

				while (delivered.load(std::memory_order_relaxed) < total)
				{
					const EPopResult result = queue.Pop(value);
					if (result == EPopResult::Element)
					{
						if (value >= total || seen[value].exchange(1, std::memory_order_relaxed) != 0)
						{
							damaged.fetch_add(1, std::memory_order_relaxed);
						}

						delivered.fetch_add(1, std::memory_order_relaxed);

						continue;
					}

					if (result == EPopResult::Blocked)
					{
						blockedAnswers.fetch_add(1, std::memory_order_relaxed);
					}

					if (std::chrono::steady_clock::now() > deadline)
					{
						stalled.store(true, std::memory_order_relaxed);

						return;
					}

					std::this_thread::yield();
				}
			});

			start.store(true, std::memory_order_release);

			for (auto& thread : threads)
			{
				thread.join();
			}

			consumer.join();

			refused += refusals.load(std::memory_order_relaxed);
			blocked += blockedAnswers.load(std::memory_order_relaxed);

			if (damaged.load(std::memory_order_relaxed) != 0)
			{
				ls << "Round " << round << ": " << damaged.load(std::memory_order_relaxed)
				   << " value(s) came out twice or outside the range the producers could send." << lferr;

				return;
			}

			if (stalled.load(std::memory_order_relaxed))
			{
				ls << "Round " << round << ": delivered " << delivered.load(std::memory_order_relaxed) << " of "
				   << total << " with grow=" << queue.GetGrowCount() << " retire=" << queue.GetRetireCount()
				   << " deferred=" << queue.GetDeferredBlockCount() << " of " << maxBlocks
				   << " blocks, fuse refusals=" << queue.GetFuseRefusalCount()
				   << ", producer refusals=" << refusals.load(std::memory_order_relaxed)
				   << ", Blocked=" << blockedAnswers.load(std::memory_order_relaxed) << "." << lferr;

				return;
			}
		}

		ls << "Pass, refused " << refused << " push(es) and answered Blocked " << blocked << " time(s)." << lf;
	});

	AddTest("A Producer Thread Is Tracked In Every Queue It Pushes", [this](auto& ls)
	{
		using Queue = TSysQueue<uint32_t, 4>;

		Queue first(4, 2);
		Queue second(4, 2);
		Queue shared(4, 2);

		std::atomic<bool> firstDone{false};
		std::atomic<bool> secondDone{false};
		std::atomic<bool> proceed{false};

		const auto waitWhileFalse = [](const std::atomic<bool>& flag)
		{
			while (!flag.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}
		};

		std::thread producerA([&first, &shared, &firstDone, &proceed, &waitWhileFalse]()
		{
			(void) first.Push(1);
			firstDone.store(true, std::memory_order_release);
			waitWhileFalse(proceed);
			(void) shared.Push(2);
		});

		std::thread producerB([&second, &shared, &secondDone, &proceed, &waitWhileFalse]()
		{
			(void) second.Push(1);
			secondDone.store(true, std::memory_order_release);
			waitWhileFalse(proceed);
			(void) shared.Push(2);
		});

		waitWhileFalse(firstDone);
		waitWhileFalse(secondDone);
		proceed.store(true, std::memory_order_release);

		producerA.join();
		producerB.join();

		bool thirdAccepted = true;
		std::thread producerC([&shared, &thirdAccepted]() { thirdAccepted = shared.Push(3) == EPushResult::Pushed; });
		producerC.join();

		if (thirdAccepted)
		{
			ls << "A third producer thread was tracked by a queue that had already given its two slots to two "
				  "others, so a thread that pushed into one queue was treated as tracked in a queue it had never "
				  "touched: two producers can share one hazard slot, and the shadow that keeps a block alive is "
				  "then written by both of them."
			   << lferr;

			return;
		}

		ls << "Pass" << lf;
	});
}
} // namespace hbe
#endif // TEST_ENABLED
