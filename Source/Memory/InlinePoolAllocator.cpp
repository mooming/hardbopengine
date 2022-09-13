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

            Time::TDuration inlineTime;
            Time::TDuration stdTime;

            constexpr int testIterations = 8192;
            constexpr size_t vectorSize = 128;

            {
                Time::Measure measure(inlineTime);

                std::vector<int, InlinePoolAllocator<int, BufferSize>> v;

                for (int j = 0; j < testIterations; ++j)
                {
                    for (int i = 0; i < vectorSize; ++i)
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
                Time::Measure measure(stdTime);
                std::vector<int> v;

                for (int j = 0; j < testIterations; ++j)
                {
                    for (int i = 0; i < vectorSize; ++i)
                    {
                        v.reserve(i + 1);
                        v.push_back(i);
                        v.shrink_to_fit();
                    }

                    v.clear();
                    v.shrink_to_fit();
                }
            }

            auto inlineTimeSec = Time::ToMilliSec(inlineTime);
            auto stdTimeSec = Time::ToMilliSec(stdTime);
            auto rate = inlineTimeSec / stdTimeSec;
            
            log.Out([inlineTimeSec, stdTimeSec, rate](auto& ls)
            {
                ls << "Vector Growth Performance : InlineAlloc: " << inlineTimeSec
                    << " msec vs STL: " << stdTimeSec << " msec, rate = " << rate;
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
        InlinePoolAllocator<int, 16> allocator;
        auto ptr = allocator.allocate(0);
        allocator.deallocate(ptr, 0);
    }

    {
        auto allocDeallocTest = [&log](auto& inlineAlloc) -> bool
        {
            constexpr int testCount = 500;
            constexpr int loopLength = 1024;
            
            log.Out([&inlineAlloc, testCount, loopLength](auto& ls)
            {
                ls << '[' << inlineAlloc.GetBlockSize() << "] Alloc/Dealloc Test: num iteration = "
                    << testCount << ", loop length = " << loopLength;
            });
            
            std::allocator<int> stdAlloc;

            Time::TDuration inlineTime;
            Time::TDuration stdTime;

            {
                Time::Measure measure(inlineTime);
                
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
                Time::Measure measure(stdTime);
                for (int j = 0; j < testCount; ++j)
                {
                    for (int i = 0; i < loopLength; ++i)
                    {
                        auto ptr = stdAlloc.allocate(i);
                        stdAlloc.deallocate(ptr, i);
                    }
                }
            }
            
            auto inlineTimeSec = Time::ToMilliSec(inlineTime);
            auto stdTimeSec = Time::ToMilliSec(stdTime);
            auto rate = inlineTimeSec / stdTimeSec;
            
            log.Out([&inlineAlloc, inlineTimeSec, stdTimeSec, rate](auto& ls)
            {
                ls << "Alloc/Dealloc Performance : InlineAlloc: "
                    << inlineTimeSec
                    << " msec vs STL: " << stdTimeSec
                    << " msec, rate = [" << rate
                    << "], fallback count = " << inlineAlloc.GetFallbackCount()
                    << " / " << (testCount * loopLength);
            });
            
            return rate < 1.0f;
        };
        
        {
            constexpr int inlineSize = 1;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 16;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 32;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 64;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 128;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }

        {
            constexpr int inlineSize = 256;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }

        {
            constexpr int inlineSize = 512;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 1024;
            InlinePoolAllocator<int, inlineSize> inlineAlloc;
            allocDeallocTest(inlineAlloc);
        }
        
        {
            constexpr int inlineSize = 8192;
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
