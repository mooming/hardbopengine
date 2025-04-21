// Created by mooming.go@gmail.com

#include "InlinePoolAllocator.h"

#ifdef __UNIT_TEST__
#include "System/ScopedTime.h"
#include <vector>

namespace
{
template <int BufferSize>
class VectorGrowthTest final
{
public:
    float operator()(const char* name, auto& ls, auto& lf) const
    {
        using namespace HE;

        Time::TDuration inlineTime;
        Time::TDuration stdTime;

        constexpr int testIterations = 8192;
        constexpr int vectorSize = 128;

        {
            Time::ScopedTime measure(inlineTime);

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
            Time::ScopedTime measure(stdTime);
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

        auto inlineTimeSec = Time::ToFloat(inlineTime);
        auto stdTimeSec = Time::ToFloat(stdTime);
        auto rate = inlineTimeSec / stdTimeSec;

        ls << "Vector Growth Performance : InlineAlloc: " << inlineTimeSec
           << " msec vs STL: " << stdTimeSec << " msec, rate = " << rate << lf;

        return rate;
    }
};
} // namespace

void HE::InlinePoolAllocatorTest::Prepare()
{
    AddTest("Zero-sized Allocation & Deallocation", [](auto&) {
        InlinePoolAllocator<int, 16> allocator;

        auto ptr = allocator.allocate(0);
        allocator.deallocate(ptr, 0);
    });

    auto allocDeallocTest = [](auto& inlineAlloc, auto& ls, auto& lf) -> float {
        constexpr int testCount = 500;
        constexpr int loopLength = 1024;

        ls << '[' << inlineAlloc.GetBlockSize()
           << "] Num Iterations = " << testCount
           << ", Loop Length = " << loopLength << lf;

        std::allocator<int> stdAlloc;

        Time::TDuration inlineTime;
        Time::TDuration stdTime;

        {
            Time::ScopedTime measure(inlineTime);

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
            Time::ScopedTime measure(stdTime);
            for (int j = 0; j < testCount; ++j)
            {
                for (int i = 0; i < loopLength; ++i)
                {
                    auto ptr = stdAlloc.allocate(i);
                    stdAlloc.deallocate(ptr, i);
                }
            }
        }

        auto inlineTimeSec = Time::ToFloat(inlineTime);
        auto stdTimeSec = Time::ToFloat(stdTime);
        auto rate = inlineTimeSec / stdTimeSec;

#ifdef PROFILE_ENABLED
        auto& mmgr = MemoryManager::GetInstance();
        auto stat = mmgr.GetAllocatorStat(inlineAlloc.GetID());

        ls << "Performance: " << inlineTimeSec << " msec vs STL: " << stdTimeSec
           << " msec, rate = [" << rate
           << "], fallback count = " << stat.fallbackCount << " / "
           << (testCount * loopLength) << lf;
#else  // PROFILE_ENABLED
        ls << "Performance: " << inlineTimeSec << " msec vs STL: " << stdTimeSec
           << " msec, rate = [" << rate << ']' << lf;
#endif // PROFILE_ENABLED

        return rate;
    };

    AddTest("Perf. & Stress, Size(1)", [&, this](auto& ls) {
        constexpr int inlineSize = 1;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(16)", [&, this](auto& ls) {
        constexpr int inlineSize = 16;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(32)", [&, this](auto& ls) {
        constexpr int inlineSize = 32;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(64)", [&, this](auto& ls) {
        constexpr int inlineSize = 64;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(128)", [&, this](auto& ls) {
        constexpr int inlineSize = 128;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(256)", [&, this](auto& ls) {
        constexpr int inlineSize = 256;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(512)", [&, this](auto& ls) {
        constexpr int inlineSize = 512;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(1024)", [&, this](auto& ls) {
        constexpr int inlineSize = 1024;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(2048)", [&, this](auto& ls) {
        constexpr int inlineSize = 2048;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Perf. & Stress, Size(8192)", [&, this](auto& ls) {
        constexpr int inlineSize = 8192;
        InlinePoolAllocator<int, inlineSize> inlineAlloc;

        auto rate = allocDeallocTest(inlineAlloc, ls, lf);
        if (rate > 1.0f)
        {
            ls << "Performance is worse than system malloc. rate = " << rate
               << lfwarn;
        }
    });

    AddTest("Vector Growth, size(32)", [&, this](auto& ls) {
        constexpr int inlineSize = 32;
        ls << "==========================================" << lf;
        ls << "Inline Alloc Size = " << inlineSize << lf;

        auto rate = VectorGrowthTest<inlineSize>()(GetName(), ls, lf);
        if (rate > 1.0f)
        {
            ls << "It's slower than system malloc. rate = " << rate << lfwarn;
        }
    });

    AddTest("Vector Growth, size(128)", [&, this](auto& ls) {
        constexpr int inlineSize = 128;
        ls << "==========================================" << lf;
        ls << "Inline Alloc Size = " << inlineSize << lf;

        auto rate = VectorGrowthTest<inlineSize>()(GetName(), ls, lf);
        if (rate > 1.0f)
        {
            ls << "It's slower than system malloc. rate = " << rate << lfwarn;
        }
    });

    AddTest("Vector Growth, size(512)", [&, this](auto& ls) {
        constexpr int inlineSize = 512;
        ls << "==========================================" << lf;
        ls << "Inline Alloc Size = " << inlineSize << lf;

        auto rate = VectorGrowthTest<inlineSize>()(GetName(), ls, lf);
        if (rate > 1.0f)
        {
            ls << "It's slower than system malloc. rate = " << rate << lfwarn;
        }
    });
}
#endif // __UNIT_TEST__
