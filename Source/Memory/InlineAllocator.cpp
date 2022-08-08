// Created by mooming.go@gmail.com, 2022

#include "InlineAllocator.h"


#ifdef __UNIT_TEST__
#include "System/Time.h"
#include <memory>
#include <vector>


bool HE::InlineAllocatorTest::DoTest()
{
	using namespace std;
    
    struct TestBlock final
    {
        int x[16];
    };
    
    int failCount = 0;
    
    {
        InlineAllocator<TestBlock, 0> allocator;
        auto ptr = allocator.allocate(0);
        allocator.deallocate(ptr, 0);
    }
    
    {
        InlineAllocator<TestBlock, 16> allocator;
        auto ptr = allocator.allocate(0);
        allocator.deallocate(ptr, 0);
    }
    
    {
        auto allocDeallocTest = [](auto& inlineAlloc) -> bool
        {
            constexpr int testCount = 1024 * 100;
            constexpr int loopLength = 64;
            
            cout << "[InlineAllocator] Alloc/Dealloc Test: num iteration = "
                << testCount << ", loop length = " << loopLength << endl;
            
            std::allocator<TestBlock> stdAlloc;
            
            float inlineTime = 0.0f;
            float stdTime = 0.0f;
            
            
            {
                Time::MeasureSec measure(inlineTime);
                
                for (int j = 0; j < testCount; ++j)
                {
                    for (int i = 0; i < loopLength; ++i)
                    {
                        auto ptr = inlineAlloc.allocate(i);
                        inlineAlloc.deallocate(ptr, i);
                    }
                }
            }
            
            {
                Time::MeasureSec measure(stdTime);
                for (int j = 0; j < testCount; ++j)
                {
                    for (int i = 0; i < loopLength; ++i)
                    {
                        auto ptr = stdAlloc.allocate(i);
                        stdAlloc.deallocate(ptr, i);
                    }
                }
            }
            
            auto rate = inlineTime / stdTime;
            cout << "[InlineAllocator] Alloc/Dealloc Performance : InlineAlloc: " << inlineTime
                << " msec vs STL: " << stdTime << " msec, rate = " << rate << endl;
            
            return rate < 1.0f;
        };
        
        auto allocDeallocTest2 = [](auto& inlineAlloc) -> bool
        {
            constexpr int testCount = 1024 * 100;
            constexpr int loopLength = 16;
            
            cout << "[InlineAllocator] Alloc/Dealloc Test: num iteration = " << testCount
                << ", loop length = " << loopLength << endl;
            
            std::allocator<TestBlock> stdAlloc;
            
            float inlineTime = 0.0f;
            float stdTime = 0.0f;
            
            
            {
                Time::MeasureSec measure(inlineTime);
                
                for (int j = 0; j < testCount; ++j)
                {
                    TestBlock* ptrs[loopLength];
                    for (int i = 0; i < loopLength; ++i)
                    {
                        ptrs[i] = inlineAlloc.allocate(i);
                    }
                    
                    for (int i = 0; i < loopLength; ++i)
                    {
                        inlineAlloc.deallocate(ptrs[i], i);
                    }
                }
            }
            
            {
                Time::MeasureSec measure(stdTime);
                for (int j = 0; j < testCount; ++j)
                {
                    TestBlock* ptrs[loopLength];
                    for (int i = 0; i < loopLength; ++i)
                    {
                        ptrs[i] = stdAlloc.allocate(i);
                    }
                    
                    for (int i = 0; i < loopLength; ++i)
                    {
                        stdAlloc.deallocate(ptrs[i], i);
                    }
                }
            }
            
            auto rate = inlineTime / stdTime;
            cout << "[InlineAllocator] Alloc/Dealloc Performance (2) : InlineAlloc: " << inlineTime
                << " msec vs STL: " << stdTime << " msec, rate = " << rate << endl;
            
            return rate < 1.0f;
        };
        
        {
            constexpr int inlineSize = 0;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                allocDeallocTest(inlineAlloc);
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                allocDeallocTest2(inlineAlloc);
            }
        }
        
        {
            constexpr int inlineSize = 16;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                allocDeallocTest(inlineAlloc);
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                allocDeallocTest2(inlineAlloc);
            }
        }
        
        {
            constexpr int inlineSize = 32;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                allocDeallocTest(inlineAlloc);
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                allocDeallocTest2(inlineAlloc);
            }
        }
        
        {
            constexpr int inlineSize = 64;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest(inlineAlloc))
                    ++failCount;
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest2(inlineAlloc))
                    ++failCount;
            }
        }
        
        {
            constexpr int inlineSize = 128;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest(inlineAlloc))
                    ++failCount;
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest2(inlineAlloc))
                    ++failCount;
            }
        }
        
        {
            constexpr int inlineSize = 1024;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest(inlineAlloc))
                    ++failCount;
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest2(inlineAlloc))
                    ++failCount;
            }
        }
        
        {
            constexpr int inlineSize = 8192;
            cout << "[InlineAllocator] ==========================================" << endl;
            cout << "[InlineAllocator] Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize << endl;
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest(inlineAlloc))
                    ++failCount;
            }
            
            {
                InlineAllocator<TestBlock, inlineSize> inlineAlloc;
                if (!allocDeallocTest2(inlineAlloc))
                    ++failCount;
            }
        }
    }
    
	return failCount <= 0;
}
#endif // __UNIT_TEST__
