// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AtomicFixedRingMPMC.h"

#ifdef TEST_ENABLED
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
		if (arrived == nullptr)
		{
			return;
		}

		arrived->store(true, std::memory_order_release);

		if (unblock == nullptr)
		{
			return;
		}
		while (!unblock->load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}
	}

	StallingValue& operator=(const StallingValue&) = default;
	StallingValue& operator=(StallingValue&&) noexcept = default;
	~StallingValue() = default;
};

struct TaggedItem
{
	std::uint32_t producer = 0;
	std::uint32_t sequence = 0;
	std::uint32_t checksum = 0;

	[[nodiscard]] bool IsConsistent() const noexcept
	{
		return checksum == producer * 1000003u + sequence;
	}
};

constexpr std::uint32_t MakeChecksum(const std::uint32_t producer, const std::uint32_t sequence) noexcept
{
	return producer * 1000003u + sequence;
}

[[nodiscard]] bool AdvanceSequence(std::atomic<std::uint32_t>& ioExpected, const std::uint32_t sequence) noexcept
{
	for (int attempt = 0; attempt < 2000000; ++attempt)
	{
		std::uint32_t current = ioExpected.load(std::memory_order_acquire);
		if (current == sequence)
		{
			if (ioExpected.compare_exchange_strong(current, sequence + 1, std::memory_order_acq_rel))
			{
				return true;
			}

			continue;
		}

		if (sequence < current)
		{
			return false;
		}

		std::this_thread::yield();
	}

	return false;
}
} // namespace

void AtomicFixedRingMPMCTest::Prepare()
{
	AddTest("Push and Pop Preserve FIFO Order", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPMC<int, 4>;

		Queue queue;
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
		using Queue = AtomicFixedRingMPMC<int, 4>;

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

			return;
		}
	});

	AddTest("Drain Then Refill Reuses Every Slot", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPMC<int, 4>;

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
				if (queue.Pop(out) != EPopResult::Element)
				{
					ls << "Lap " << round << " ran dry after three pushes." << lferr;

					return;
				}

				if (out != round * 10 + offset)
				{
					ls << "Lap " << round << " delivered " << out << ", expected " << round * 10 + offset << "."
					   << lferr;

					return;
				}
			}
		}
	});

	AddTest("An Unpublished Claim Is Invisible, Never an Element", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPMC<StallingValue, 4>;

		Queue queue;
		std::atomic<bool> arrived(false);
		std::atomic<bool> unblock(false);

		StallingValue stalled(7, &arrived, &unblock);
		std::thread producer([&queue, &stalled]
		{
			const auto result = queue.Push(stalled);
			(void) result;
		});

		const bool parked = WaitUntil([&arrived] { return arrived.load(std::memory_order_acquire); }, 2000);

		StallingValue early;
		const bool answeredEmpty = queue.Pop(early) == EPopResult::Empty;

		unblock.store(true, std::memory_order_release);

		StallingValue recovered;
		bool delivered = false;
		for (int attempt = 0; attempt < 2000 && !delivered; ++attempt)
		{
			delivered = queue.Pop(recovered) == EPopResult::Element;
			continueIf(delivered);
			std::this_thread::yield();
		}

		producer.join();

		if (!parked)
		{
			ls << "The producer never entered the claim-then-publish window, so the window was not tested." << lferr;

			return;
		}

		if (!answeredEmpty)
		{
			ls << "Pop reported an element whose copy constructor had not returned." << lferr;

			return;
		}

		if (!delivered || recovered.serial != 7)
		{
			ls << "The stalled element was lost rather than delayed." << lferr;

			return;
		}
	});

	AddTest("Concurrent Push Delivers Each Item Exactly Once", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPMC<TaggedItem, 16>;

		constexpr int producerCount = 4;
		constexpr int consumerCount = 2;
		constexpr std::uint32_t perProducer = 5000;
		constexpr std::uint32_t total = producerCount * perProducer;

		Queue queue;
		std::atomic<std::uint32_t> pushed(0);
		std::atomic<std::uint32_t> popped(0);
		std::atomic<std::uint32_t> violations(0);
		std::atomic<std::uint32_t> expected[producerCount];

		for (auto& slot : expected)
		{
			slot.store(0, std::memory_order_relaxed);
		}

		std::vector<std::thread> threads;
		for (int producer = 0; producer < producerCount; ++producer)
		{
			threads.emplace_back([&queue, &pushed, producer]
			{
				for (std::uint32_t sequence = 0; sequence < perProducer; ++sequence)
				{
					TaggedItem item;
					item.producer = static_cast<std::uint32_t>(producer);
					item.sequence = sequence;
					item.checksum = MakeChecksum(item.producer, sequence);

					while (queue.Push(item) != EPushResult::Pushed)
					{
						std::this_thread::yield();
					}

					pushed.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		for (int consumer = 0; consumer < consumerCount; ++consumer)
		{
			threads.emplace_back([&queue, &popped, &violations, &expected]
			{
				TaggedItem item;
				while (popped.load(std::memory_order_relaxed) < total)
				{
					if (queue.Pop(item) == EPopResult::Empty)
					{
						std::this_thread::yield();
						continue;
					}

					if (!item.IsConsistent() || item.producer >= producerCount)
					{
						violations.fetch_add(1, std::memory_order_relaxed);

						return;
					}

					auto& next = expected[item.producer];
					if (!AdvanceSequence(next, item.sequence))
					{
						violations.fetch_add(1, std::memory_order_relaxed);

						return;
					}

					popped.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		const bool finished = WaitUntil([&popped, &pushed, &violations]
		{
			return violations.load(std::memory_order_relaxed) > 0 ||
				   (popped.load(std::memory_order_relaxed) >= total && pushed.load(std::memory_order_relaxed) >= total);
		}, 10000);

		for (auto& thread : threads)
		{
			thread.join();
		}

		if (violations.load(std::memory_order_relaxed) > 0)
		{
			ls << "Concurrent delivery broke an invariant: " << violations.load() << " violation(s)." << lferr;

			return;
		}

		if (!finished || popped.load() != total || pushed.load() != total)
		{
			ls << "Concurrent run delivered " << popped.load() << " of " << total << " pushed items (" << pushed.load()
			   << " pushed, " << (finished ? "watchdog passed" : "watchdog fired") << ")." << lferr;
		}
	});
}
} // namespace hbe

#endif // TEST_ENABLED
