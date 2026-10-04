// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AtomicQueueViewMultiProviderSingleConsumer.h"

#ifdef __UNIT_TEST__
#include <array>
#include <atomic>
#include <thread>

namespace hbe
{
namespace
{
struct QueueNode
{
	QueueNode* next = nullptr;
	int producer = -1;
	int serial = -1;
	int token = -1;
};

void PushThreeThenDestroyWithoutDraining(std::array<QueueNode, 3>& nodes)
{
	AtomicQueueViewMPSC<QueueNode> queue;

	for (auto& node : nodes)
	{
		queue.Push(node);
	}

	queue.Pop();
}
} // namespace

void AtomicQueueViewMultiProviderSingleConsumerTest::Prepare()
{
	AddTest("Push and Pop Preserve FIFO Order", [this](auto& ls)
	{
		std::array<QueueNode, 5> nodes{};

		for (int index = 0; index < 5; ++index)
		{
			nodes[index].serial = index;
		}

		AtomicQueueViewMPSC<QueueNode> queue;

		if (!queue.IsEmpty())
		{
			ls << "A fresh queue reported work it never received." << lferr;

			return;
		}

		for (auto& node : nodes)
		{
			queue.Push(node);
		}

		if (queue.IsEmpty())
		{
			ls << "Five nodes were pushed and the queue still calls itself empty." << lferr;

			return;
		}

		int expected = 0;

		while (auto* node = queue.Pop())
		{
			if (node->serial != expected)
			{
				ls << "Position " << expected << " came out as serial " << node->serial
				   << ", so the batch reversal did not restore first-in first-out order." << lferr;

				return;
			}

			if (node->next != nullptr)
			{
				ls << "Pop handed serial " << node->serial << " back still linked into a chain." << lferr;

				return;
			}

			++expected;
		}

		if (expected != 5 || !queue.IsEmpty() || queue.Pop() != nullptr)
		{
			ls << "Draining returned " << expected << " node(s) of 5." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("IsEmpty Covers the Stolen Batch as Well as the Producer Stack", [this](auto& ls)
	{
		std::array<QueueNode, 3> nodes{};
		AtomicQueueViewMPSC<QueueNode> queue;

		for (int index = 0; index < 3; ++index)
		{
			nodes[index].serial = index;
			queue.Push(nodes[index]);
		}

		auto* first = queue.Pop();

		if (first == nullptr || first->serial != 0)
		{
			ls << "The first pop did not return the first node pushed." << lferr;

			return;
		}

		if (queue.IsEmpty())
		{
			ls << "Two nodes are still owed, but that pop moved them out of the producer stack into the consumer's "
				  "private list, so an emptiness test that reads only the producer stack calls the queue empty while "
				  "it still holds work."
			   << lferr;

			return;
		}

		int expected = 1;

		while (auto* node = queue.Pop())
		{
			if (node->serial != expected++)
			{
				ls << "Out of order after the batch was stolen." << lferr;

				return;
			}
		}

		if (expected != 3 || !queue.IsEmpty())
		{
			ls << "Expected all 3 nodes and then an empty queue." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});

	AddTest("Four Producers and One Consumer Lose Nothing", [this](auto& ls)
	{
		constexpr int producerCount = 4;
		constexpr int perProducer = 1000;
		constexpr int nodeTotal = producerCount * perProducer;

		std::array<QueueNode, nodeTotal> nodes{};
		std::array<bool, nodeTotal> seen{};
		std::array<int, producerCount> lastSerial{};

		lastSerial.fill(-1);

		AtomicQueueViewMPSC<QueueNode> queue;
		std::atomic<bool> startRunning{false};

		for (int index = 0; index < nodeTotal; ++index)
		{
			nodes[index].producer = index / perProducer;
			nodes[index].serial = index % perProducer;
		}

		int received = 0;
		int outOfOrder = 0;
		int duplicates = 0;

		auto producer = [&](int slot)
		{
			while (!startRunning.load(std::memory_order_acquire))
			{
				std::this_thread::yield();
			}

			for (int index = slot * perProducer; index < (slot + 1) * perProducer; ++index)
			{
				queue.Push(nodes[index]);
			}
		};

		std::array<std::thread, producerCount> threads;

		for (int slot = 0; slot < producerCount; ++slot)
		{
			threads[slot] = std::thread(producer, slot);
		}

		startRunning.store(true, std::memory_order_release);

		const bool drainedEverything = WaitUntil([&]()
		{
			for (int spin = 0; spin < 64; ++spin)
			{
				while (auto* node = queue.Pop())
				{
					const int flatIndex = node->producer * perProducer + node->serial;

					if (seen[flatIndex])
					{
						++duplicates;
					}
					else
					{
						seen[flatIndex] = true;
					}

					if (node->serial <= lastSerial[node->producer])
					{
						++outOfOrder;
					}

					lastSerial[node->producer] = node->serial;
					++received;
				}
			}

			return received >= nodeTotal;
		});

		for (auto& thread : threads)
		{
			thread.join();
		}
		while (auto* node = queue.Pop())
		{
			const int flatIndex = node->producer * perProducer + node->serial;

			if (seen[flatIndex])
			{
				++duplicates;
			}
			else
			{
				seen[flatIndex] = true;
			}

			++received;
		}

		ls << "multi-producer stress: " << producerCount << " producer(s), " << perProducer << " node(s) each, "
		   << received << " delivered, " << outOfOrder << " out of per-producer order, " << duplicates
		   << " delivered twice." << lf;

		if (!drainedEverything)
		{
			ls << "Only " << received << " of " << nodeTotal
			   << " nodes arrived before the deadline, so a node was "
				  "left unreachable by the links."
			   << lferr;
		}

		if (outOfOrder != 0)
		{
			ls << "A producer's own nodes came out in the wrong order, which means the chain it pushed onto was "
				  "relinked under it."
			   << lferr;
		}

		if (duplicates != 0)
		{
			ls << "The same node was handed out twice, so the batch reversal left two links to it." << lferr;
		}
	});

	AddTest("A Popped Node Belongs to Its Owner and to No One Else", [this](auto& ls)
	{
		constexpr int nodeCount = 32;
		constexpr int roundCount = 150;

		std::array<QueueNode, nodeCount> nodes{};
		std::atomic<bool> startRunning{false};

		int handedBack = 0;
		int clobbered = 0;
		int handedTwice = 0;
		int strandedRound = -1;

		for (int index = 0; index < nodeCount; ++index)
		{
			nodes[index].serial = index;
		}

		for (int round = 0; round < roundCount; ++round)
		{
			AtomicQueueViewMPSC<QueueNode> queue;

			auto producer = [&](int slot)
			{
				while (!startRunning.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				for (int index = slot; index < nodeCount; index += 2)
				{
					queue.Push(nodes[index]);
				}
			};

			std::thread first(producer, 0);
			std::thread second(producer, 1);

			startRunning.store(true, std::memory_order_release);

			int returned = 0;

			const bool allReturned = WaitUntil([&]()
			{
				for (int spin = 0; spin < 512 && returned < nodeCount; ++spin)
				{
					auto* node = queue.Pop();

					if (node == nullptr)
					{
						std::this_thread::yield();

						continue;
					}

					node->next = nullptr;
					node->token = round;

					std::this_thread::yield();

					if (node->token != round)
					{
						++clobbered;
					}

					node->token = -1;
					++returned;
					++handedBack;
				}

				return returned >= nodeCount;
			}, 200);

			first.join();
			second.join();

			if (!allReturned)
			{
				strandedRound = round;

				break;
			}

			if (queue.Pop() != nullptr)
			{
				++handedTwice;
			}

			startRunning.store(false, std::memory_order_release);
		}

		ls << "sole-ownership stress: " << handedBack << " node(s) handed back and held out of "
		   << roundCount * nodeCount << ", " << clobbered << " written into by a stranger, " << handedTwice
		   << " round(s) where a drained queue still had a node." << lf;

		if (strandedRound >= 0)
		{
			ls << "Round " << strandedRound << " never saw all " << nodeCount
			   << " nodes before its deadline, so a node was left unreachable by the links." << lferr;
		}

		if (clobbered != 0)
		{
			ls << "A node was still being written by somebody else after Pop had handed it back, which is the defect "
				  "that makes a queue unsafe to reuse storage in: whoever pops cannot own what it holds."
			   << lferr;
		}

		if (handedTwice != 0)
		{
			ls << "Every node was already accounted for and the queue still had one to give." << lferr;
		}
	});

	AddTest("Destroying a Queue That Still Holds Nodes Releases Nothing", [this](auto& ls)
	{
		std::array<QueueNode, 3> nodes{};

		for (int index = 0; index < 3; ++index)
		{
			nodes[index].serial = 100 + index;
		}

		PushThreeThenDestroyWithoutDraining(nodes);

		AtomicQueueViewMPSC<QueueNode> reuse;

		for (auto& node : nodes)
		{
			reuse.Push(node);
		}

		int checked = 0;
		bool damaged = false;

		while (auto* node = reuse.Pop())
		{
			if (node->serial < 100 || node->serial > 102)
			{
				damaged = true;
			}

			++checked;
		}

		if (checked != 3 || damaged)
		{
			ls << "The queue owned nothing, so destroying it with nodes still queued must leave all 3 of the caller's "
				  "nodes intact and reusable; it returned "
			   << checked << " and reported damage: " << (damaged ? "yes" : "no") << "." << lferr;

			return;
		}

		ls << "Pass" << lf;
	});
}
} // namespace hbe
#endif // __UNIT_TEST__
