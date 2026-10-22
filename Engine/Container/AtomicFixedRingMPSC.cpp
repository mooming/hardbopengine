// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AtomicFixedRingMPSC.h"

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
} // namespace

void AtomicFixedRingMPSCTest::Prepare()
{
	AddTest("Push and Pop Preserve FIFO Order", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPSC<int, 4>;

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
		using Queue = AtomicFixedRingMPSC<int, 4>;

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
		using Queue = AtomicFixedRingMPSC<int, 4>;

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

	AddTest("An Unpublished Claim Is Invisible, Never an Element", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPSC<StallingValue, 4>;

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
		}
	});

	AddTest("Many Producers One Consumer Deliver Each Item Once", [this](auto& ls)
	{
		using Queue = AtomicFixedRingMPSC<TaggedItem, 16>;

		constexpr int producerCount = 6;
		constexpr std::uint32_t perProducer = 5000;
		constexpr std::uint32_t total = producerCount * perProducer;

		Queue queue;
		std::atomic<std::uint32_t> pushed(0);
		std::atomic<std::uint32_t> violations(0);
		std::atomic<bool> finished(false);
		std::uint32_t expected[producerCount] = {};
		std::uint32_t delivered = 0;

		std::vector<std::thread> threads;
		for (int producer = 0; producer < producerCount; ++producer)
		{
			threads.emplace_back([&queue, &pushed, &violations, producer]
			{
				for (std::uint32_t sequence = 0; sequence < perProducer; ++sequence)
				{
					TaggedItem item;
					item.producer = static_cast<std::uint32_t>(producer);
					item.sequence = sequence;
					item.checksum = item.producer * 1000003u + sequence;

					while (queue.Push(item) != EPushResult::Pushed)
					{
						if (violations.load(std::memory_order_relaxed) > 0)
						{
							return;
						}

						std::this_thread::yield();
					}

					pushed.fetch_add(1, std::memory_order_relaxed);
				}
			});
		}

		std::thread consumer([&queue, &expected, &delivered, &violations, &finished]
		{
			TaggedItem item;
			while (delivered < total)
			{
				if (queue.Pop(item) == EPopResult::Empty)
				{
					if (violations.load(std::memory_order_relaxed) > 0)
					{
						return;
					}

					std::this_thread::yield();
					continue;
				}

				if (!item.IsConsistent() || item.producer >= producerCount || expected[item.producer] != item.sequence)
				{
					violations.fetch_add(1, std::memory_order_relaxed);

					return;
				}

				++expected[item.producer];
				++delivered;
			}

			finished.store(true, std::memory_order_release);
		});

		const bool drained = WaitUntil([&finished, &violations]
		{ return finished.load(std::memory_order_acquire) || violations.load(std::memory_order_relaxed) > 0; }, 10000);

		consumer.join();
		for (auto& thread : threads)
		{
			thread.join();
		}

		if (violations.load(std::memory_order_relaxed) > 0)
		{
			ls << "Single-consumer delivery broke an invariant: " << violations.load() << " violation(s)." << lferr;

			return;
		}

		if (!drained || delivered != total)
		{
			ls << "Single-consumer run delivered " << delivered << " of " << total << " items ("
			   << (drained ? "watchdog passed" : "watchdog fired") << ")." << lferr;
		}
	});
}
} // namespace hbe

#endif // TEST_ENABLED
