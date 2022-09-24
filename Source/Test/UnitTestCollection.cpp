// Created by mooming.go@gmail.com, 2017

#ifdef __UNIT_TEST__

#include "UnitTestCollection.h"

#include "TestEnv.h"
#include "Container/LinkedList.h"
#include "Math/MathUtil.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Matrix2x2.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"
#include "Math/Quaternion.h"
#include "Math/Transform.h"
#include "Math/UniformTransform.h"
#include "Math/RigidTransform.h"
#include "Math/AABB.h"
#include "Memory/BaseAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include "Memory/Optional.h"
#include "Memory/PoolAllocator.h"
#include "Memory/MultiPoolAllocator.h"
#include "Memory/StackAllocator.h"
#include "Memory/SystemAllocator.h"
#include "String/StaticString.h"
#include "String/StringUtil.h"
#include "System/ComponentSystem.h"
#include <iostream>


using namespace HE;

namespace Test
{
    void RunUnitTests()
    {
        using namespace std;

        cout << fixed;
        cout.precision(4);

        auto& testEnv = TestEnv::GetEnv();

        testEnv.AddTestCollection(new SystemAllocatorTest());
        testEnv.AddTestCollection(new BaseAllocatorTest());
        testEnv.AddTestCollection(new InlinePoolAllocatorTest());
        testEnv.AddTestCollection(new StackAllocatorTest());
        testEnv.AddTestCollection(new PoolAllocatorTest());
        testEnv.AddTestCollection(new MultiPoolAllocatorTest());
        
        testEnv.AddTestCollection(new ArrayTest());
        testEnv.AddTestCollection(new LinkedListTest());
        testEnv.AddTestCollection(new OptionalTest());
        
        testEnv.AddTestCollection(new StaticStringTest());
        testEnv.AddTestCollection(new StringTest());
        testEnv.AddTestCollection(new StringUtilTest());
        
        testEnv.AddTestCollection(new MathUtilTest());
        testEnv.AddTestCollection(new Vector2Test());
        testEnv.AddTestCollection(new Vector3Test());
        testEnv.AddTestCollection(new Vector4Test());
        testEnv.AddTestCollection(new Matrix3x3Test());
        testEnv.AddTestCollection(new QuaternionTest());
        testEnv.AddTestCollection(new UniformTransformTest());
        testEnv.AddTestCollection(new RigidTransformTest());
        testEnv.AddTestCollection(new AABBTest());
        testEnv.AddTestCollection(new TransformTest());
        
        testEnv.AddTestCollection(new ComponentSystemTest());
        
        testEnv.Start();
    }
}

#endif // __UNIT_TEST__
