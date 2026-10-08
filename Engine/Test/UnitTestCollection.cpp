// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __TEST__

#include <numeric>
#include <vector>

#include "UnitTestCollection.h"

#include "Container/Array.h"
#include "Container/AtomicStackView.h"
#include "Container/BoundedPriorityQueue.h"
#include "Container/Deque.h"
#include "Container/HashMap.h"
#include "Container/LinkedList.h"
#include "Container/Map.h"
#include "Container/Queue.h"
#include "Container/RingQueue.h"
#include "Container/Vector.h"
#include "Core/CPUBudget.h"
#include "Core/ComponentSystem.h"
#include "Core/Debug.h"
#include "Core/ResultPacket.h"
#include "Core/StreamDrainPolicy.h"
#include "Core/TaskProvider.h"
#include "Core/TaskRegistry.h"
#include "Core/TaskStream.h"
#include "Core/TaskSystem.h"
#include "Core/Time.h"
#include "Engine/Engine.h"
#include "HSTL/HUnorderedMap.h"
#include "Math/AABB.h"
#include "Math/ImportanceResampling.h"
#include "Math/MathUtil.h"
#include "Math/Matrix3x3.h"
#include "Math/MonteCarloIntegrator.h"
#include "Math/PerlinNoise.h"
#include "Math/Quaternion.h"
#include "Math/RigidTransform.h"
#include "Math/StratifiedSampling.h"
#include "Math/Transform.h"
#include "Math/UniformTransform.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Memory/DefaultAllocator.h"
#include "Memory/InlineMonotonicAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include "Memory/MemoryManager.h"
#include "Memory/MonotonicAllocator.h"
#include "Memory/MultiPoolAllocator.h"
#include "Memory/Optional.h"
#include "Memory/PoolAllocator.h"
#include "Memory/StackAllocator.h"
#include "Memory/SystemAllocator.h"
#include "Memory/ThreadSafeMultiPoolAllocator.h"
#include "OSAL/OSDebug.h"
#include "OSAL/OSInputOutput.h"
#include "OSAL/OSThread.h"
#include "OSAL/Window.h"
#include "Renderer/RHICapabilities.h"
#include "RendererTest.h"
#include "Resource/Buffer.h"
#include "Resource/BufferInputStream.h"
#include "Resource/BufferOutputStream.h"
#include "String/InlineStringBuilder.h"
#include "String/StaticString.h"
#include "String/StringBuilder.h"
#include "String/StringUtil.h"
#include "TestEnv.h"

namespace hbe::Test
{
namespace
{
std::vector<std::size_t> testletIndices;

std::size_t RunTestletTask(void* userData, std::size_t, std::size_t);
std::size_t ReportAndShutDownTask(void*, std::size_t, std::size_t);

void PostSuiteTask(const std::size_t testletIndex)
{
	auto& testEnv = TestEnv::GetEnv();
	auto& taskSystem = Engine::Get().GetTaskSystem();

	const bool isLast = testletIndex >= testEnv.GetTestletCount();
	const char* label = isLast ? "Suite Report" : testEnv.GetTestletLabel(testletIndex);
	auto* taskFunc = isLast ? &ReportAndShutDownTask : &RunTestletTask;
	void* userData = isLast ? nullptr : static_cast<void*>(&testletIndices[testletIndex]);

	const auto taskID = taskSystem.CreateTask(label, taskFunc, userData);

	auto* task = taskSystem.FindTask(taskID);
	FatalAssert(task != nullptr, "Suite step ", label,
				" could not be created as a task, so the suite cannot be driven to its registered end");
	if (task == nullptr)
	{
		return;
	}

	taskSystem.EnqueueTask(TaskSystem::GetBaseTaskStreamIndex(), *task);
}

std::size_t RunTestletTask(void* userData, std::size_t, std::size_t)
{
	auto& testEnv = TestEnv::GetEnv();

	// Asked of the stream before the testlet runs, because this is a question about who is driving, and a testlet
	// that has already reported cannot answer it. The engine loop driving this item means the shutdown pump has
	// not started; if it has, the loop is dead or absent and whatever follows was produced by the rescue path -
	// which is the run that must not be allowed to look like a pass.
	if (Engine::Get().GetTaskSystem().GetStream(TaskSystem::GetBaseTaskStreamIndex()).IsDrivenByShutdownPump())
	{
		testEnv.NoteSuiteDrivenByShutdownPump();
	}

	const auto testletIndex = *static_cast<std::size_t*>(userData);

	{
		MultiPoolAllocator allocator("Testlet");
		AllocatorScope scope(allocator);

		testEnv.RunTestlet(testletIndex);
	}

	PostSuiteTask(testletIndex + 1);

	return 1;
}

std::size_t ReportAndShutDownTask(void*, std::size_t, std::size_t)
{
	TestEnv::GetEnv().Finalize();

	Engine::Get().ShutDown();

	return 1;
}
} // namespace

void RegisterSuite()
{
	auto& testEnv = TestEnv::GetEnv();

	testEnv.AddTestCollection<SystemAllocatorTest>();
	testEnv.AddTestCollection<BaseAllocatorTest>();
	testEnv.AddTestCollection<InlinePoolAllocatorTest>();
	testEnv.AddTestCollection<InlineMonotonicAllocatorTest>();
	testEnv.AddTestCollection<StackAllocatorTest>();
	testEnv.AddTestCollection<PoolAllocatorTest>();
	testEnv.AddTestCollection<MonotonicAllocatorTest>();
	testEnv.AddTestCollection<MultiPoolAllocatorTest>();
	testEnv.AddTestCollection<ThreadSafeMultiPoolAllocatorTest>();
	testEnv.AddTestCollection<GlobalAllocationTest>();
	testEnv.AddTestCollection<OSDebugTest>();
	testEnv.AddTestCollection<OSInputOutputTest>();
	testEnv.AddTestCollection<OSThreadTest>();
	testEnv.AddTestCollection<TimeTest>();
	testEnv.AddTestCollection<CPUBudgetTest>();
	testEnv.AddTestCollection<TaskProviderTest>();
	testEnv.AddTestCollection<TaskRegistryTest>();
	testEnv.AddTestCollection<ResultPacketTest>();
	testEnv.AddTestCollection<StreamDrainPolicyTest>();
	testEnv.AddTestCollection<WindowTest>();
	testEnv.AddTestCollection<OSMemoryTest>();
	testEnv.AddTestCollection<RendererTest>();
	testEnv.AddTestCollection<BufferTest>();
	testEnv.AddTestCollection<BufferInputStreamTest>();
	testEnv.AddTestCollection<BufferOutputStreamTest>();
	testEnv.AddTestCollection<HUnorderedMapTest>();
	testEnv.AddTestCollection<ArrayTest>();
	testEnv.AddTestCollection<BoundedPriorityQueueTest>();
	testEnv.AddTestCollection<AtomicStackViewTest>();
	testEnv.AddTestCollection<LinkedListTest>();
	testEnv.AddTestCollection<VectorTest>();
	testEnv.AddTestCollection<MapTest>();
	testEnv.AddTestCollection<HashMapTest>();
	testEnv.AddTestCollection<DequeTest>();
	testEnv.AddTestCollection<QueueTest>();
	testEnv.AddTestCollection<RingQueueTest>();
	testEnv.AddTestCollection<OptionalTest>();
	testEnv.AddTestCollection<StaticStringTest>();
	testEnv.AddTestCollection<StringTest>();
	testEnv.AddTestCollection<InlineStringBuilderTest>();
	testEnv.AddTestCollection<StringBuilderTest>();
	testEnv.AddTestCollection<StringUtilTest>();

	testEnv.AddTestCollection<MathUtilTest>();
	testEnv.AddTestCollection<Vector2Test>();
	testEnv.AddTestCollection<Vector3Test>();
	testEnv.AddTestCollection<Vector4Test>();
	testEnv.AddTestCollection<MonteCarloIntegrationTest>();
	testEnv.AddTestCollection<StratifiedSamplingTest>();
	testEnv.AddTestCollection<ImportanceResamplingTest>();

	testEnv.AddTestCollection<Matrix3x3Test>();
	testEnv.AddTestCollection<QuaternionTest>();
	testEnv.AddTestCollection<UniformTransformTest>();
	testEnv.AddTestCollection<RigidTransformTest>();
	testEnv.AddTestCollection<AABBTest>();
	testEnv.AddTestCollection<TransformTest>();
	testEnv.AddTestCollection<PerlinNoiseTest>();

	testEnv.AddTestCollection<ComponentSystemTest>();
	testEnv.AddTestCollection<TaskStreamAffinityTest>();
	testEnv.AddTestCollection<TaskSystemTest>();
	testEnv.AddTestCollection<RHICapabilitiesTest>();

	testEnv.PrepareTestlets();
}

void ScheduleSuiteOnBaseStream()
{
	auto& testEnv = TestEnv::GetEnv();

	// Each slot holds its own index, and a testlet task is handed the address of its slot: a work item's payload is a
	// bare void*, so the index has to live somewhere that outlives the item that reads it. Resizing alone would leave
	// every slot zero, and a suite where every step is told it is step zero re-runs its first testlet forever - which
	// is what this looked like the first time, until the task registry ran out of records and named it.
	testletIndices.resize(testEnv.GetTestletCount());
	std::iota(testletIndices.begin(), testletIndices.end(), 0);

	PostSuiteTask(0);
}

} // namespace hbe::Test

#endif // __TEST__
