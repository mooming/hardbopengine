// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "FixedRingMPMC.h"

#ifdef TEST_ENABLED
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace hbe
{
namespace
{
[[nodiscard]] bool TryMark(std::atomic<std::uint16_t>* const marks, const std::uint32_t id) noexcept
{
	std::uint16_t expected = 0;

	return id != 0xFFFFFFFFu && marks[id].compare_exchange_strong(expected, 1, std::memory_order_relaxed);
}
} // namespace

void FixedRingMPMCTest::Prepare()
{
	AddTest("Push and Pop Preserve FIFO Order", [this](auto& ls)
	{
		using Queue = FixedRingMPMC<int, 4>;

		Queue queue;
		if (!queue.IsEmpty())
		{
			ls << "A fresh queue reported work it never received." << lferr;

			return;
		}

		for (int value = 0; value < 30; ++value)
		{
			if (queue.Push(value) != EPushResult::Pushed)
			{
				ls << "Push refused item " << value << " while draining a single consumer." << lferr;

				return;
			}

			int out = -1;
			if (queue.Pop(out) != EPopResult::Element || out != value)
			{
				ls << "Pop returned " << out << " for the item pushed as " << value << "." << lferr;

				return;
			}
		}
	});

	AddTest("Capacity Is Answered Without Losing an Item", [this](auto& ls)
	{
		using Queue = FixedRingMPMC<int, 4>;

		Queue queue;
		for (int value = 0; value < static_cast<int>(Queue::GetCapacity()); ++value)
		{
			if (queue.Push(value) != EPushResult::Pushed)
			{
				ls << "Push refused item " << value << " of " << Queue::GetCapacity() << "." << lferr;

				return;
			}
		}

		if (queue.Push(static_cast<int>(Queue::GetCapacity())) != EPushResult::Full)
		{
			ls << "Push accepted a " << Queue::GetCapacity() + 1 << "th item into a ring of " << Queue::GetCapacity()
			   << "." << lferr;

			return;
		}

		int out = -1;
		if (queue.Pop(out) != EPopResult::Element || out != 0)
		{
			ls << "A full ring refused to release its oldest item." << lferr;

			return;
		}

		if (queue.Push(static_cast<int>(Queue::GetCapacity())) != EPushResult::Pushed)
		{
			ls << "One freed slot did not make room for the item that had been refused." << lferr;
		}
	});

	AddTest("Drain Then Refill Reuses Every Slot", [this](auto& ls)
	{
		using Queue = FixedRingMPMC<int, 4>;

		Queue queue;
		for (int round = 0; round < 200; ++round)
		{
			for (int offset = 0; offset < 3; ++offset)
			{
				if (queue.Push(round * 10 + offset) != EPushResult::Pushed)
				{
					ls << "Lap " << round << " refused its " << offset << "th item." << lferr;

					return;
				}
			}

			for (int offset = 0; offset < 3; ++offset)
			{
				int out = -1;
				if (queue.Pop(out) != EPopResult::Element || out != round * 10 + offset)
				{
					ls << "Lap " << round << " delivered " << out << ", expected " << round * 10 + offset << "."
					   << lferr;

					return;
				}
			}
		}
	});

	AddTest("Concurrent Push Delivers Each Item Exactly Once", [this](auto& ls)
	{
		using Queue = FixedRingMPMC<std::uint32_t, 16>;

		constexpr int producerCount = 4;
		constexpr int consumerCount = 2;
		constexpr std::uint32_t perProducer = 2500;
		constexpr std::uint32_t total = producerCount * perProducer;

		Queue queue;
		std::unique_ptr<std::atomic<std::uint16_t>[]> marks(new std::atomic<std::uint16_t>[total]());
		std::atomic<std::uint32_t> delivered(0);
		std::atomic<std::uint32_t> pushed(0);
		std::atomic<std::uint32_t> violations(0);

		std::vector<std::thread> threads;
		for (int producer = 0; producer < producerCount; ++producer)
		{
			threads.emplace_back([&queue, &pushed, producer]
			{
				for (std::uint32_t sequence = 0; sequence < perProducer; ++sequence)
				{
					const auto id = static_cast<std::uint32_t>(producer) * perProducer + sequence;
					while (queue.Push(id) != EPushResult::Pushed)
					{
						std::this_thread::yield();
					}

					pushed.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		for (int consumer = 0; consumer < consumerCount; ++consumer)
		{
			threads.emplace_back([&queue, &marks, &delivered, &violations]
			{
				std::uint32_t id = 0xFFFFFFFFu;
				while (delivered.load(std::memory_order_relaxed) < total)
				{
					if (queue.Pop(id) == EPopResult::Empty)
					{
						std::this_thread::yield();
						continue;
					}

					if (!TryMark(marks.get(), id))
					{
						violations.fetch_add(1, std::memory_order_relaxed);

						return;
					}

					delivered.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		const bool finished = WaitUntil([&delivered, &pushed, &violations]
		{
			return violations.load(std::memory_order_relaxed) > 0 ||
				   (delivered.load(std::memory_order_relaxed) >= total &&
					pushed.load(std::memory_order_relaxed) >= total);
		}, 10000);

		for (auto& thread : threads)
		{
			thread.join();
		}

		std::uint32_t marked = 0;
		for (std::uint32_t id = 0; id < total; ++id)
		{
			marked += marks[id].load(std::memory_order_relaxed);
		}

		if (violations.load(std::memory_order_relaxed) > 0)
		{
			ls << "Concurrent delivery duplicated or invented an item: " << violations.load() << " violation(s)."
			   << lferr;

			return;
		}

		if (!finished || marked != total || delivered.load() != total)
		{
			ls << "Concurrent run marked " << marked << " distinct ids and delivered " << delivered.load() << " of "
			   << total << (finished ? "." : ", and the run did not finish in time.") << lferr;
		}
	});
}
} // namespace hbe

#endif // TEST_ENABLED
