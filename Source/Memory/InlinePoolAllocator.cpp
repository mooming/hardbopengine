// Created by mooming.go@gmail.com, 2022

#include "InlinePoolAllocator.h"


#ifdef __UNIT_TEST__
#include "Log/Logger.h"
#include "System/Time.h"
#include <memory>
#include <vector>


namespace
{
    template <int BufferSize>
    class VectorGrowthTest final
    {
    public:
        bool operator() (const char* name) const
        {
            HE::TLog log(name);
            
            using namespace HE;

            float inlineTime = 0.0f;
            float stdTime = 0.0f;

            constexpr int testIterations = 8192;
            constexpr size_t vectorSize = 128;

            {
                Time::MeasureSec measure(inlineTime);

                std::vector<int, InlinePoolAllocator<int, BufferSize>> v;

                for (int j = 0; j < testIterations; ++j)
                {
                    for (size_t i = 0; i < vectorSize; ++i)
                    {
                        v.reserve(i + 1);
                        v.push_back(i);
                        v.shrink_to_fit();
                    }

                    v.clear();
                    v.shrink_to_fit();
                }
            }

            {
                Time::MeasureSec measure(stdTime);
                std::vector<int> v;

                for (int j = 0; j < testIterations; ++j)
                {
                    for (size_t i = 0; i < vectorSize; ++i)
                    {
                        v.reserve(i + 1);
                        v.push_back(i);
                        v.shrink_to_fit();
                    }

                    v.clear();
                    v.shrink_to_fit();
                }
            }

            auto rate = inlineTime / stdTime;
            
            log.Out([inlineTime, stdTime, rate](auto& ls)
            {
                ls << "Vector Growth Performance : InlineAlloc: " << inlineTime
                    << " msec vs STL: " << stdTime << " msec, rate = " << rate;
            });
            
            return rate < 1.0f;
        }
    };
} // anonymous

bool HE::InlinePoolAllocatorTest::DoTest()
{
	using namespace std;
    
    TLog log(GetName());
    
    int failCount = 0;

    {
        InlinePoolAllocator<int, 0> allocator;
        auto ptr = allocator.allocate(0);
        allocator.deallocate(ptr, 0);
    }

    {
        InlinePoolAllocator<int, 16> allocator;
        auto ptr = allocator.allocate(0);
        allocator.deallocate(ptr, 0);
    }

    {
        auto allocDeallocTest = [&log](auto& inlineAlloc) -> bool
        {
            constexpr int testCount = 1000;
            constexpr int loopLength = 64;
            
            log.Out([testCount, loopLength](auto& ls)
            {
                ls << "Alloc/Dealloc Test: num iteration = "
                    << testCount << ", loop length = " << loopLength;
            });
            
            std::allocator<int> stdAlloc;
            
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
            
            log.Out([inlineTime, stdTime, rate](auto& ls)
            {
                ls << "Alloc/Dealloc Performance : InlineAlloc: " << inlineTime
                    << " msec vs STL: " << stdTime << " msec, rate = " << rate;
            });
            
            return rate < 1.0f;
        };
        
        {
            constexpr int inlineSize = 0;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 16;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 32;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 64;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 128;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 1024;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 8192;
            
            log.Out([inlineSize](auto& ls)
            {
                ls << "==========================================" << endl;
                ls << "Alloc/Dealloc Test: Inline Alloc Size = " << inlineSize;
            });
            
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
    }

    {
        constexpr int inlineSize = 32;
        
        log.Out([inlineSize](auto& ls)
        {
            ls << "==========================================" << endl;
            ls << "Vector Growth Test: Inline Alloc Size = " << inlineSize;
        });
        
        VectorGrowthTest<inlineSize>()(GetName());
    }

    {
        constexpr int inlineSize = 64;
        
        log.Out([inlineSize](auto& ls)
        {
            ls << "==========================================" << endl;
            ls << "Vector Growth Test: Inline Alloc Size = " << inlineSize;
        });
        
        VectorGrowthTest<inlineSize>()(GetName());
    }

    {
        constexpr int inlineSize = 128;
        
        log.Out([inlineSize](auto& ls)
        {
            ls << "==========================================" << endl;
            ls << "Vector Growth Test: Inline Alloc Size = " << inlineSize;
        });
        
        VectorGrowthTest<inlineSize>()(GetName());
    }

    {
        constexpr int inlineSize = 256;
        
        log.Out([inlineSize](auto& ls)
        {
            ls << "==========================================" << endl;
            ls << "Vector Growth Test: Inline Alloc Size = " << inlineSize;
        });
        
        VectorGrowthTest<inlineSize>()(GetName());
    }

	return failCount <= 0;
}
#endif // __UNIT_TEST__
