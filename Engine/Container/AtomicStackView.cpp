// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "AtomicStackView.h"

#ifdef TEST_ENABLED
#include <array>
#include <atomic>
#include <thread>

#include "HSTL/HVector.h"
#include "Log/Logger.h"

namespace hbe
{
namespace
{
template <typename T>
class TNode final
{
public:
	T value;
	TNode* next;

	TNode(const T& value)
		: value(value)
		, next(nullptr)
	{
	}
};
} // namespace

void AtomicStackViewTest::Prepare()
{
	AddTest("Default Constructor", [this](auto& ls)
	{
		{
			AtomicStackView<TNode<bool>> stack;
			ls << "Bool Stack: [Done]" << lf;
		}
		{
			AtomicStackView<TNode<char>> stack;
			ls << "Char Stack: [Done]" << lf;
		}
		{
			AtomicStackView<TNode<int>> stack;
			ls << "Int Stack: [Done]" << lf;
		}
		{
			AtomicStackView<TNode<float>> stack;
			ls << "Float Stack: [Done]" << lf;
		}
		{
			AtomicStackView<TNode<double>> stack;
			ls << "Double Stack: [Done]" << lf;
		}
	});

	AddTest("Push", [this](auto& ls)
	{
		AtomicStackView<TNode<bool>> stack;

		TNode<bool> boolValues[] = {true, true, true, false, false, false};

		for (auto& node : boolValues)
		{
			auto& value = node.value;

			ls << "Push Input = " << value << lf;
			stack.Push(node);
		}
		while (auto node = stack.Pop())
		{
			if (node == nullptr)
			{
				ls << "Encounters a null node!" << lferr;
				continue;
			}

			auto& value = node->value;
			ls << "Pop Output = " << value << lf;
		}
	});

	AddTest("Thread-Safety", [this](auto& ls)
	{
		constexpr int NumItem = 1000;
		constexpr int NumLoop = 500;

		hbe::HVector<TNode<int>> values;
		values.reserve(NumItem);

		for (int i = 0; i < NumItem; ++i)
		{
			values.push_back(i);
		}

		AtomicStackView<TNode<int>> stack;

		std::atomic<int> pushCount = 0;
		auto PushFunc = [&]()
		{
			for (auto& node : values)
			{
				stack.Push(node);
				pushCount.fetch_add(1, std::memory_order_relaxed);
			}
		};

		std::atomic<int> popCount = 0;
		auto PopFunc = [&]()
		{
			int count = 0;
			while (!stack.IsEmpty())
			{
				if (stack.Pop())
				{
					++count;
				}
			}

			popCount.fetch_add(count, std::memory_order_relaxed);
		};

		hbe::HVector<std::thread> threads;
		values.reserve(5);

		for (int j = 0; j < NumLoop; ++j)
		{
			threads.emplace_back(PushFunc);
			threads.emplace_back(PopFunc);
			threads.emplace_back(PopFunc);
			threads.emplace_back(PopFunc);
			threads.emplace_back(PopFunc);

			for (auto& thread : threads)
			{
				thread.join();
			}

			threads.clear();

			PopFunc();

			ls << "Iteration: " << j << ", push count = " << pushCount.load(std::memory_order_relaxed)
			   << ", pop count = " << popCount.load(std::memory_order_relaxed) << lf;
		}

		ls << "Multithreaded push & pop test done!" << lf;
	});

	AddTest("Recycled nodes are never handed to two threads at once", [this](auto& ls)
	{
		struct StressNode
		{
			StressNode* next = nullptr;
			std::atomic<bool> claimed{false};
			int serial = -1;
		};

		constexpr int nodeCount = 128;
		constexpr int roundCount = 400;
		constexpr int popsPerThread = 4;

		std::array<StressNode, nodeCount> nodes{};

		for (int index = 0; index < nodeCount; ++index)
		{
			nodes[index].serial = index;
		}

		AtomicStackView<StressNode> stack;

		std::atomic<int> duplicateHandoffs{0};
		std::atomic<int> totalPops{0};
		std::atomic<int> threadPops[2]{};
		std::atomic<bool> roundOpen{false};

		int contendedRounds = 0;
		int lostNodes = 0;
		int aliasedNodes = 0;

		for (int round = 0; round < roundCount; ++round)
		{
			for (auto& node : nodes)
			{
				node.claimed.store(false, std::memory_order_relaxed);
			}

			for (auto& node : nodes)
			{
				stack.Push(node);
			}

			roundOpen.store(false, std::memory_order_relaxed);

			auto consumer = [&](int slot)
			{
				while (!roundOpen.load(std::memory_order_acquire))
				{
					std::this_thread::yield();
				}

				int popped = 0;

				while (popped < popsPerThread)
				{
					auto* node = stack.Pop();

					if (node == nullptr)
					{
						continue;
					}

					totalPops.fetch_add(1, std::memory_order_relaxed);

					if (node->claimed.exchange(true, std::memory_order_acq_rel))
					{
						duplicateHandoffs.fetch_add(1, std::memory_order_relaxed);
					}

					++popped;

					node->claimed.store(false, std::memory_order_release);
					stack.Push(*node);
				}

				threadPops[slot].store(popped, std::memory_order_relaxed);
			};

			std::thread first(consumer, 0);
			std::thread second(consumer, 1);

			roundOpen.store(true, std::memory_order_release);

			first.join();
			second.join();

			if (threadPops[0].load(std::memory_order_relaxed) > 0 && threadPops[1].load(std::memory_order_relaxed) > 0)
			{
				++contendedRounds;
			}

			std::array<bool, nodeCount> seen{};
			int drained = 0;

			while (auto* node = stack.Pop())
			{
				if (seen[node->serial])
				{
					++aliasedNodes;
				}
				else
				{
					seen[node->serial] = true;
					++drained;
				}
			}

			lostNodes += nodeCount - drained;
		}

		ls << "recycled-node stress: " << roundCount << " round(s), " << nodeCount << " node(s), " << totalPops.load()
		   << " pop(s), " << contendedRounds << " contended round(s), " << duplicateHandoffs.load()
		   << " double hand-off(s), " << lostNodes << " lost reference(s), " << aliasedNodes << " aliased pop(s)."
		   << lf;

		if (contendedRounds < roundCount / 2)
		{
			ls << "Both threads only actually popped in " << contendedRounds << " of " << roundCount
			   << " rounds, so most rounds observed no contention at all." << lferr;
		}

		if (duplicateHandoffs.load() != 0)
		{
			ls << "A node was claimed while another thread still held it: the same node reached two threads at once, "
				  "which "
				  "is exactly what a plain next field read by a non-owner allows."
			   << lferr;
		}

		if (lostNodes != 0)
		{
			ls << "References went missing: the stack held fewer than " << nodeCount
			   << " nodes after quiescence, so a CAS that overwrote a stale next lost a link." << lferr;
		}

		if (aliasedNodes != 0)
		{
			ls << "The same node came out twice at quiescence, so the list had a cycle or a duplicate link." << lferr;
		}
	});
}
} // namespace hbe
#endif //TEST_ENABLED
