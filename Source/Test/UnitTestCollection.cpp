// Created by mooming.go@gmail.com, 2017

#ifdef __UNIT_TEST__

#include "UnitTestCollection.h"

#include "TestEnv.h"
#include "Container/Vector.h"
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
#include "Memory/InlineAllocator.h"
#include "Memory/InlinePoolAllocator.h"
#include "Memory/Optional.h"
#include "Memory/PoolAllocator.h"
#include "Memory/StackAllocator.h"
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

        testEnv.AddTest(new StackAllocatorTest());
        testEnv.AddTest(new BaseAllocatorTest());
        testEnv.AddTest(new InlineAllocatorTest());
        testEnv.AddTest(new InlinePoolAllocatorTest());
        testEnv.AddTest(new PoolAllocatorTest());
        
        testEnv.AddTest(new ArrayTest());
        testEnv.AddTest(new LinkedListTest());
        testEnv.AddTest(new OptionalTest());
        
        testEnv.AddTest(new StringTest());
        testEnv.AddTest(new StringUtilTest());
        
        testEnv.AddTest(new MathUtilTest());
        testEnv.AddTest(new VectorTest());
        testEnv.AddTest(new Vector2Test());
        testEnv.AddTest(new Vector3Test());
        testEnv.AddTest(new Vector4Test());
        testEnv.AddTest(new Matrix3x3Test());
        testEnv.AddTest(new QuaternionTest());
        testEnv.AddTest(new UniformTransformTest());
        testEnv.AddTest(new RigidTransformTest());
        testEnv.AddTest(new AABBTest());
        testEnv.AddTest(new TransformTest());
        
        testEnv.AddTest(new ComponentSystemTest());
        
        testEnv.Start();
    }
}

#endif // __UNIT_TEST__
