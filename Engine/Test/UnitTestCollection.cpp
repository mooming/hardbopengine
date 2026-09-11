// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __UNIT_TEST__

#include "UnitTestCollection.h"

#include "RendererTest.h"
#include "Container/Array.h"
#include "Container/AtomicStackView.h"
#include "Container/BoundedPriorityQueue.h"
#include "Container/LinkedList.h"
#include "Container/Vector.h"
#include "Container/Map.h"
#include "Container/HashMap.h"
#include "Container/Deque.h"
#include "Container/Queue.h"
#include "Container/RingQueue.h"
#include "Core/ComponentSystem.h"
#include "Core/TaskSystem.h"
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

void runTests()
{
	auto testFunc = [](void*, std::size_t, std::size_t) -> std::size_t
	{
		MultiPoolAllocator allocator("UnitTest");
		AllocatorScope scope(allocator);

		auto& testEnv = TestEnv::getEnv();

		testEnv.addTestCollection<SystemAllocatorTest>();
		testEnv.addTestCollection<BaseAllocatorTest>();
		testEnv.addTestCollection<InlinePoolAllocatorTest>();
		testEnv.addTestCollection<InlineMonotonicAllocatorTest>();
		testEnv.addTestCollection<StackAllocatorTest>();
		testEnv.addTestCollection<PoolAllocatorTest>();
		testEnv.addTestCollection<MonotonicAllocatorTest>();
		testEnv.addTestCollection<MultiPoolAllocatorTest>();
		testEnv.addTestCollection<ThreadSafeMultiPoolAllocatorTest>();
		testEnv.addTestCollection<OSDebugTest>();
		testEnv.addTestCollection<OSInputOutputTest>();
		testEnv.addTestCollection<OSThreadTest>();
		testEnv.addTestCollection<WindowTest>();
		testEnv.addTestCollection<OSMemoryTest>();
		testEnv.addTestCollection<RendererTest>();
		testEnv.addTestCollection<BufferTest>();
		testEnv.addTestCollection<BufferInputStreamTest>();
		testEnv.addTestCollection<BufferOutputStreamTest>();
		testEnv.addTestCollection<HUnorderedMapTest>();
		testEnv.addTestCollection<ArrayTest>();
		testEnv.addTestCollection<BoundedPriorityQueueTest>();
		testEnv.addTestCollection<AtomicStackViewTest>();
		testEnv.addTestCollection<LinkedListTest>();
		testEnv.addTestCollection<VectorTest>();
		testEnv.addTestCollection<MapTest>();
		testEnv.addTestCollection<HashMapTest>();
		testEnv.addTestCollection<DequeTest>();
		testEnv.addTestCollection<QueueTest>();
		testEnv.addTestCollection<RingQueueTest>();
		testEnv.addTestCollection<OptionalTest>();
		testEnv.addTestCollection<StaticStringTest>();
		testEnv.addTestCollection<StringTest>();
		testEnv.addTestCollection<InlineStringBuilderTest>();
		testEnv.addTestCollection<StringBuilderTest>();
		testEnv.addTestCollection<StringUtilTest>();

		testEnv.addTestCollection<MathUtilTest>();
		testEnv.addTestCollection<Vector2Test>();
		testEnv.addTestCollection<Vector3Test>();
		testEnv.addTestCollection<Vector4Test>();
		testEnv.addTestCollection<MonteCarloIntegrationTest>();
		testEnv.addTestCollection<StratifiedSamplingTest>();
		testEnv.addTestCollection<ImportanceResamplingTest>();

		testEnv.addTestCollection<Matrix3x3Test>();
		testEnv.addTestCollection<QuaternionTest>();
		testEnv.addTestCollection<UniformTransformTest>();
		testEnv.addTestCollection<RigidTransformTest>();
		testEnv.addTestCollection<AABBTest>();
		testEnv.addTestCollection<TransformTest>();
		testEnv.addTestCollection<PerlinNoiseTest>();

		testEnv.addTestCollection<ComponentSystemTest>();
		testEnv.addTestCollection<TaskStreamAffinityTest>();
		testEnv.addTestCollection<TaskSystemTest>();
		testEnv.addTestCollection<RHICapabilitiesTest>();

		testEnv.start();

		Engine::get().shutDown();

		return 1;
	};

	static Task task("TestEnv", testFunc, nullptr);

	auto rangedTask = task.generateSubTask(0, 1, 0);
	auto& taskSystem = Engine::get().getTaskSystem();

	const auto baseStreamIndex = TaskSystem::getBaseTaskStreamIndex();
	taskSystem.enqueue(baseStreamIndex, rangedTask);
}

} // namespace hbe::Test

#endif // __UNIT_TEST__
