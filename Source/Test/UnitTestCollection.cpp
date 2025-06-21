// Created by mooming.go@gmail.com, 2017

#ifdef __UNIT_TEST__

#include "UnitTestCollection.h"

#include "Container/Array.h"
#include "Container/AtomicStackView.h"
#include "Container/LinkedList.h"
#include "HSTL/HUnorderedMap.h"
#include "Math/AABB.h"
#include "Math/MathUtil.h"
#include "Math/Matrix3x3.h"
#include "Math/MonteCarloIntegrator.h"
#include "Math/Quaternion.h"
#include "Math/RigidTransform.h"
#include "Math/Transform.h"
#include "Math/UniformTransform.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Memory/BaseAllocator.h"
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
#include "Resource/Buffer.h"
#include "Resource/BufferInputStream.h"
#include "Resource/BufferOutputStream.h"
#include "String/InlineStringBuilder.h"
#include "String/StaticString.h"
#include "String/StringBuilder.h"
#include "String/StringUtil.h"
#include "System/ComponentSystem.h"
#include "System/TaskSystem.h"
#include "TestEnv.h"
#include <iostream>

using namespace HE;

namespace Test
{
void RunUnitTests()
{
    MultiPoolAllocator allocator("UnitTest");
    AllocatorScope scope(allocator);

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

    testEnv.AddTestCollection<OSDebugTest>();
    testEnv.AddTestCollection<OSInputOutputTest>();
    testEnv.AddTestCollection<OSThreadTest>();

    testEnv.AddTestCollection<BufferTest>();
    testEnv.AddTestCollection<BufferInputStreamTest>();
    testEnv.AddTestCollection<BufferOutputStreamTest>();

    testEnv.AddTestCollection<HUnorderedMapTest>();

    testEnv.AddTestCollection<ArrayTest>();
    testEnv.AddTestCollection<AtomicStackViewTest>();
    testEnv.AddTestCollection<LinkedListTest>();
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

    testEnv.AddTestCollection<Matrix3x3Test>();
    testEnv.AddTestCollection<QuaternionTest>();
    testEnv.AddTestCollection<UniformTransformTest>();
    testEnv.AddTestCollection<RigidTransformTest>();
    testEnv.AddTestCollection<AABBTest>();
    testEnv.AddTestCollection<TransformTest>();

    testEnv.AddTestCollection<ComponentSystemTest>();
    testEnv.AddTestCollection<TaskSystemTest>();

    testEnv.Start();
}
} // namespace Test

#endif // __UNIT_TEST__
