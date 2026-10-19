// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RingBufferPerf.h"

#ifdef TEST_ENABLED
#include <atomic>
#include <chrono>
#include <cstdint>
#include <new>
#include <thread>
#include <vector>

#include "Config/BuildConfig.h"
#include "Container/AtomicFixedRingMPMC.h"
#include "Container/AtomicFixedRingMPSC.h"
#include "Container/EPopResult.h"
#include "Container/EPushResult.h"
#include "Container/FixedRingMPMC.h"
#include "Container/FixedRingMPSC.h"
#include "Container/RingBuffer.h"
#include "Core/CommonMacros.h"
#include "Memory/SystemAllocator.h"
#include "Test/PerfHarness.h"

namespace hbe
{
namespace
{
using Test::PerfBenchmark;
using Test::PerfPlatformHeader;
using Test::PerfSample;

struct PerfPayload
{
	std::size_t producerIndex;
	std::uint32_t sequence;
	std::uint32_t check;
};

struct PerfShape
{
	std::size_t producers;
	std::size_t consumers;
	std::size_t itemsPerProducer;
	std::size_t arrivalGapNanoseconds;
};

enum class ECellKind : unsigned char
{
	Mpsc,
	Mpmc,
	SpinCap,
	Chain
};

constexpr std::uint32_t CheckPrime = 0x9E3779B9U;

constexpr std::size_t SaturatedItemCount = 200000;
constexpr std::size_t SparseItemCount = 10000;
constexpr std::size_t SparseGapNanoseconds = 2000;
constexpr std::size_t CompactRounds = 5;
constexpr std::size_t SweepRounds = 5;

constexpr std::size_t MpscLowProducerMinRatioPercent = 200;
constexpr std::size_t MpscManyProducerMinRatioPercent = 120;

[[nodiscard]] constexpr std::uint32_t CheckOf(const std::size_t inProducer, const std::size_t inSequence) noexcept
{
	return static_cast<std::uint32_t>(inProducer * CheckPrime) ^ static_cast<std::uint32_t>(inSequence * 0x85EBCA6BU);
}

[[nodiscard]] constexpr bool IsOptimizedBuild() noexcept
{
	return RELEASE_BUILD != 0;
}

[[nodiscard]] constexpr PerfShape SaturatedShape(const std::size_t inProducers, const std::size_t inConsumers) noexcept
{
	return PerfShape{inProducers, inConsumers, SaturatedItemCount / inProducers, 0};
}

[[nodiscard]] constexpr PerfShape SparseShape(const std::size_t inProducers, const std::size_t inConsumers) noexcept
{
	return PerfShape{inProducers, inConsumers, SparseItemCount, SparseGapNanoseconds};
}

template <typename T, std::size_t Capacity>
class RingMPSCFetchAdd final
{
public:
	static constexpr std::size_t IndexMask = Capacity - 1;

private:
	static_assert(Capacity > 1 && (Capacity & (Capacity - 1)) == 0,
				  "Capacity must be a power of two, so a slot index is one mask and no division reaches the hot path.");

	static_assert(sizeof(T) % alignof(T) == 0,
				  "sizeof(T) must be a multiple of alignof(T), so a stride of sizeof(T) keeps every slot aligned.");

	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> writeIndex;
	alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> readyIndex[Capacity];
	alignas(std::hardware_destructive_interference_size) std::size_t readIndex;
	alignas(alignof(T)) std::byte payload[Capacity * sizeof(T)];

public:
	RingMPSCFetchAdd(const RingMPSCFetchAdd&) = delete;
	RingMPSCFetchAdd& operator=(const RingMPSCFetchAdd&) = delete;

	RingMPSCFetchAdd() noexcept
		: writeIndex(0)
		, readIndex(0)
	{
		for (std::size_t index = 0; index < Capacity; ++index)
		{
			readyIndex[index].store(index, std::memory_order_relaxed);
		}
	}

	~RingMPSCFetchAdd()
	{
		const std::size_t tail = writeIndex.load(std::memory_order_relaxed);

		for (std::size_t index = readIndex; index < tail; ++index)
		{
			continueIf(readyIndex[index & IndexMask].load(std::memory_order_relaxed) != index + 1);

			PayloadAt(index)->~T();
		}
	}

	[[nodiscard]] EPushResult Push(const T& inItem) noexcept
	{
		const std::size_t claim = writeIndex.fetch_add(1, std::memory_order_relaxed);

		while (readyIndex[claim & IndexMask].load(std::memory_order_acquire) != claim)
		{
			std::this_thread::yield();
		}

		new (PayloadAt(claim)) T(inItem);
		readyIndex[claim & IndexMask].store(claim + 1, std::memory_order_release);

		return EPushResult::Pushed;
	}

	[[nodiscard]] EPopResult Pop(T& outItem) noexcept
	{
		returnValueIf(EPopResult::Empty,
					  readyIndex[readIndex & IndexMask].load(std::memory_order_acquire) != readIndex + 1);

		T* element = PayloadAt(readIndex);
		outItem = std::move(*element);
		element->~T();

		readyIndex[readIndex & IndexMask].store(readIndex + Capacity, std::memory_order_release);
		++readIndex;

		return EPopResult::Element;
	}

private:
	[[nodiscard]] T* PayloadAt(const std::size_t inIndex) noexcept
	{
		return reinterpret_cast<T*>(payload + (inIndex & IndexMask) * sizeof(T));
	}
};

using TAtomicMPSC = AtomicFixedRingMPSC<PerfPayload, 64>;
using TAtomicMPMC = AtomicFixedRingMPMC<PerfPayload, 64>;
using TSpinDefault = AtomicFixedRingMPSC<PerfPayload, 64, 8>;
using TSpinNone = AtomicFixedRingMPSC<PerfPayload, 64, 0>;
using TFetchAdd = RingMPSCFetchAdd<PerfPayload, 64>;

using TChainMPSC = RingBuffer<PerfPayload, AtomicFixedRingMPSC<PerfPayload, 64>, SystemAllocator<PerfPayload>>;
using TAboveRing = AtomicFixedRingMPSC<PerfPayload, 4096>;

using TMutexMPSC = FixedRingMPSC<PerfPayload, 64>;
using TMutexMPMC = FixedRingMPMC<PerfPayload, 64>;

constexpr PerfShape MpscP1 = SaturatedShape(1, 1);
constexpr PerfShape MpscP2 = SaturatedShape(2, 1);
constexpr PerfShape MpscP4 = SaturatedShape(4, 1);
constexpr PerfShape MpscP8 = SaturatedShape(8, 1);
constexpr PerfShape MpscP16 = SaturatedShape(16, 1);

constexpr PerfShape MpmcP1 = SaturatedShape(1, 2);
constexpr PerfShape MpmcP2 = SaturatedShape(2, 2);
constexpr PerfShape MpmcP4 = SaturatedShape(4, 2);
constexpr PerfShape MpmcP8 = SaturatedShape(8, 2);
constexpr PerfShape MpmcP16 = SaturatedShape(16, 2);

constexpr PerfShape SparseMpscP1 = SparseShape(1, 1);
constexpr PerfShape SparseMpscP2 = SparseShape(2, 1);
constexpr PerfShape SparseMpscP4 = SparseShape(4, 1);
constexpr PerfShape SparseMpscP8 = SparseShape(8, 1);
constexpr PerfShape SparseMpscP16 = SparseShape(16, 1);

constexpr PerfShape SparseMpmcP2 = SparseShape(2, 2);
constexpr PerfShape SparseMpmcP4 = SparseShape(4, 2);
constexpr PerfShape SparseMpmcP8 = SparseShape(8, 2);
constexpr PerfShape SparseMpmcP16 = SparseShape(16, 2);

constexpr std::chrono::milliseconds CellTimeout{30000};
constexpr std::size_t DeadlineCheckInterval = 4096;

void WaitForArrivalGap(const std::size_t inNanoseconds)
{
	returnIf(inNanoseconds == 0);

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(inNanoseconds);

	while (std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::yield();
	}
}

template <typename TQueue>
PerfSample RunPerfCell(const void* inUserData)
{
	const PerfShape& shape = *static_cast<const PerfShape*>(inUserData);
	const std::size_t total = shape.producers * shape.itemsPerProducer;

	std::vector<std::atomic<unsigned char>> marks(total);

	for (std::atomic<unsigned char>& mark : marks)
	{
		mark.store(0, std::memory_order_relaxed);
	}

	std::atomic<std::size_t> consumed{0};
	std::atomic<std::size_t> violations{0};
	std::atomic<bool> start{false};
	std::atomic<bool> abandoned{false};

	TQueue queue;

	const auto began = std::chrono::steady_clock::now();
	const auto deadline = began + CellTimeout;

	const auto produce = [&](const std::size_t inProducer)
	{
		while (!start.load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}

		std::size_t refusals = 0;

		for (std::size_t item = 0; item < shape.itemsPerProducer; ++item)
		{
			const PerfPayload payload{inProducer, static_cast<std::uint32_t>(item), CheckOf(inProducer, item)};

			while (queue.Push(payload) != EPushResult::Pushed)
			{
				if (++refusals % DeadlineCheckInterval == 0 && std::chrono::steady_clock::now() > deadline)
				{
					abandoned.store(true, std::memory_order_relaxed);

					return;
				}

				std::this_thread::yield();
			}

			WaitForArrivalGap(shape.arrivalGapNanoseconds);
		}
	};

	const auto consume = [&]
	{
		while (!start.load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}

		PerfPayload payload{};
		std::size_t empties = 0;

		while (consumed.load(std::memory_order_relaxed) < total)
		{
			if (queue.Pop(payload) != EPopResult::Element)
			{
				if (++empties % DeadlineCheckInterval == 0 && std::chrono::steady_clock::now() > deadline)
				{
					abandoned.store(true, std::memory_order_relaxed);

					return;
				}

				continue;
			}

			const bool valid = payload.producerIndex < shape.producers &&
							   payload.check == CheckOf(payload.producerIndex, payload.sequence);
			const std::size_t id = valid ? payload.producerIndex * shape.itemsPerProducer + payload.sequence : 0;
			const bool duplicate = valid && marks[id].exchange(1, std::memory_order_relaxed) != 0;

			if (!valid || duplicate)
			{
				violations.fetch_add(1, std::memory_order_relaxed);
			}

			consumed.fetch_add(1, std::memory_order_relaxed);
		}
	};

	std::vector<std::thread> workers;
	workers.reserve(shape.producers + shape.consumers);

	for (std::size_t producer = 0; producer < shape.producers; ++producer)
	{
		workers.emplace_back(produce, producer);
	}

	for (std::size_t consumer = 0; consumer < shape.consumers; ++consumer)
	{
		workers.emplace_back(consume);
	}

	start.store(true, std::memory_order_release);

	for (std::thread& worker : workers)
	{
		worker.join();
	}

	const std::size_t elapsed = static_cast<std::size_t>(
			std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - began).count());

	std::size_t missing = 0;

	for (const std::atomic<unsigned char>& mark : marks)
	{
		continueIf(mark.load(std::memory_order_relaxed) != 0);

		++missing;
	}

	const std::size_t counted = violations.load(std::memory_order_relaxed) + missing;

	return PerfSample{elapsed, total, abandoned.load(std::memory_order_relaxed) ? counted + 1 : counted};
}

[[nodiscard]] const char* NameOf(const ECellKind inKind) noexcept
{
	switch (inKind)
	{
		case ECellKind::Mpmc:
			return "MPMC";
		case ECellKind::SpinCap:
			return "MPSC spin cap";
		case ECellKind::Chain:
			return "Chain";
		case ECellKind::Mpsc:
		default:
			return "MPSC";
	}
}

void RunShapeLine(std::ostream& ioOut, const ECellKind inKind, const PerfShape& inShape, const std::size_t inRounds,
				  const bool inClaim)
{
	ioOut << "  " << NameOf(inKind) << " | P" << inShape.producers << " C" << inShape.consumers << ", capacity 64, "
		  << inShape.itemsPerProducer << " items per producer, arrival gap " << inShape.arrivalGapNanoseconds << " ns, "
		  << inRounds << " rounds, claim " << (inClaim ? "graded" : "numbers only") << '\n';
}

void RunCell(std::ostream& ioOut, const PerfShape& inShape, const ECellKind inKind, const std::size_t inRounds,
			 const bool inControl, const bool inClaim, const bool inFetchAdd)
{
	const bool claim = inClaim && IsOptimizedBuild() && inKind != ECellKind::SpinCap;

	RunShapeLine(ioOut, inKind, inShape, inRounds, claim);

	PerfBenchmark bench(inRounds);

	if (inKind == ECellKind::SpinCap)
	{
		bench.AddArm("AtomicFixedRingMPSC spin8", &inShape, &RunPerfCell<TSpinDefault>);
		bench.AddArm("AtomicFixedRingMPSC yield", &inShape, &RunPerfCell<TSpinNone>);
		bench.AddArm("AtomicFixedRingMPSC spin8 (control)", &inShape, &RunPerfCell<TSpinDefault>);
		bench.Control("AtomicFixedRingMPSC spin8 (control)");
	}
	else if (inKind == ECellKind::Chain)
	{
		bench.AddArm("RingBuffer chain", &inShape, &RunPerfCell<TChainMPSC>);
		bench.AddArm("AtomicFixedRingMPSC (bare ring 64)", &inShape, &RunPerfCell<TAtomicMPSC>);
		bench.AddArm("AtomicFixedRingMPSC (bare ring 4096)", &inShape, &RunPerfCell<TAboveRing>);

		if (inControl)
		{
			bench.AddArm("RingBuffer chain (control)", &inShape, &RunPerfCell<TChainMPSC>);
			bench.Control("RingBuffer chain (control)");
		}
	}
	else if (inKind == ECellKind::Mpmc)
	{
		bench.AddArm("AtomicFixedRingMPMC", &inShape, &RunPerfCell<TAtomicMPMC>);
		bench.AddArm("FixedRingMPMC (mutex)", &inShape, &RunPerfCell<TMutexMPMC>);

		if (inControl)
		{
			bench.AddArm("FixedRingMPMC (control)", &inShape, &RunPerfCell<TMutexMPMC>);
			bench.Control("FixedRingMPMC (control)");
		}
	}
	else
	{
		bench.AddArm("AtomicFixedRingMPSC", &inShape, &RunPerfCell<TAtomicMPSC>);
		bench.AddArm("FixedRingMPSC (mutex)", &inShape, &RunPerfCell<TMutexMPSC>);

		if (inFetchAdd)
		{
			bench.AddArm("MPSCFetchAdd (experiment)", &inShape, &RunPerfCell<TFetchAdd>);
		}

		if (inControl)
		{
			bench.AddArm("AtomicFixedRingMPSC (control)", &inShape, &RunPerfCell<TAtomicMPSC>);
			bench.Control("AtomicFixedRingMPSC (control)");
		}

		if (claim)
		{
			const std::size_t margin =
					inShape.producers <= 2 ? MpscLowProducerMinRatioPercent : MpscManyProducerMinRatioPercent;

			bench.Expect("AtomicFixedRingMPSC", "FixedRingMPSC (mutex)", margin);
		}
	}

	bench.Run();
	bench.Report(ioOut);
}
} // namespace

RingBufferPerfTest::RingBufferPerfTest()
	: TestCollection("RingBufferPerfTest")
{
}

void RingBufferPerfTest::Prepare()
{
	const auto add =
			[this](const auto& testName, const PerfShape& testShape, const ECellKind testKind, const bool testClaim)
	{
		AddTest(testName, [this, &testShape, testKind, testClaim](TLogOut& log)
		{
			PerfPlatformHeader(log);
			RunCell(log, testShape, testKind, CompactRounds, true, testClaim, false);
			log << lf;
		});
	};

	add("MPSC Atomic Ring Beats Mutex Ring While Saturated", MpscP1, ECellKind::Mpsc, true);
	add("MPSC Atomic Ring Beats Mutex Ring At Two Producers", MpscP2, ECellKind::Mpsc, true);
	add("MPMC Mutex Ring Against Atomic Ring At Two Producers", MpmcP2, ECellKind::Mpmc, false);
}

RingBufferPerfBenchmarkTest::RingBufferPerfBenchmarkTest()
	: TestCollection("RingBufferPerfBenchmarkTest")
{
}

void RingBufferPerfBenchmarkTest::Prepare()
{
	const auto add = [this](const auto& testName, const PerfShape& testShape, const ECellKind testKind,
							const bool testClaim, const bool testFetchAdd)
	{
		AddTest(testName, [this, &testShape, testKind, testClaim, testFetchAdd](TLogOut& log)
		{
			PerfPlatformHeader(log);
			RunCell(log, testShape, testKind, SweepRounds, true, testClaim, testFetchAdd);
			log << lf;
		});
	};

	add("Saturated MPSC One Producer", MpscP1, ECellKind::Mpsc, true, false);
	add("Saturated MPSC Two Producers", MpscP2, ECellKind::Mpsc, true, false);
	add("Saturated MPSC Four Producers", MpscP4, ECellKind::Mpsc, true, true);
	add("Saturated MPSC Eight Producers", MpscP8, ECellKind::Mpsc, true, true);
	add("Saturated MPSC Sixteen Producers", MpscP16, ECellKind::Mpsc, true, true);

	add("Saturated MPMC One Producer", MpmcP1, ECellKind::Mpmc, false, false);
	add("Saturated MPMC Two Producers", MpmcP2, ECellKind::Mpmc, false, false);
	add("Saturated MPMC Four Producers", MpmcP4, ECellKind::Mpmc, false, false);
	add("Saturated MPMC Eight Producers", MpmcP8, ECellKind::Mpmc, false, false);
	add("Saturated MPMC Sixteen Producers", MpmcP16, ECellKind::Mpmc, false, false);

	add("Sparse MPSC One Producer", SparseMpscP1, ECellKind::Mpsc, false, false);
	add("Sparse MPSC Two Producers", SparseMpscP2, ECellKind::Mpsc, false, false);
	add("Sparse MPSC Four Producers", SparseMpscP4, ECellKind::Mpsc, false, false);
	add("Sparse MPSC Eight Producers", SparseMpscP8, ECellKind::Mpsc, false, false);
	add("Sparse MPSC Sixteen Producers", SparseMpscP16, ECellKind::Mpsc, false, false);

	add("Sparse MPMC Two Producers", SparseMpmcP2, ECellKind::Mpmc, false, false);
	add("Sparse MPMC Four Producers", SparseMpmcP4, ECellKind::Mpmc, false, false);
	add("Sparse MPMC Eight Producers", SparseMpmcP8, ECellKind::Mpmc, false, false);
	add("Sparse MPMC Sixteen Producers", SparseMpmcP16, ECellKind::Mpmc, false, false);

	add("Chain Over A Bare Ring One Producer", MpscP1, ECellKind::Chain, false, false);
	add("Chain Over A Bare Ring Four Producers", MpscP4, ECellKind::Chain, false, false);
	add("Sparse Chain Over A Bare Ring", SparseMpscP4, ECellKind::Chain, false, false);
	add("Spin Cap Against Yielding At Four Producers", MpscP4, ECellKind::SpinCap, false, false);
	add("Spin Cap Against Yielding At Sixteen Producers", MpscP16, ECellKind::SpinCap, false, false);
	add("Sparse Spin Cap Against Yielding", SparseMpscP16, ECellKind::SpinCap, false, false);
}
} // namespace hbe

#endif // TEST_ENABLED
