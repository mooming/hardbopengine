// Created by mooming.go@gmail.com

#include "InlinePoolAllocator.h"

#ifdef __UNIT_TEST__
#include <vector>
#include "System/ScopedTime.h"

namespace
{
	template<int BufferSize>
	class VectorGrowthTest final
	{
	public:
		float operator()(const char* name, auto& ls, auto& lf, int vectorSize) const
		{
			using namespace hbe;

			Time::TDuration inlineTime;
			Time::TDuration stdTime;

			constexpr int testIterations = 8192;

			{
				Time::ScopedTime measure(inlineTime);

				std::vector<int, InlinePoolAllocator<int, BufferSize>> v;

				for (int j = 0; j < testIterations; ++j)
				{
					for (int i = 0; i < vectorSize; ++i)
					{
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

			ls << "Vector Growth Performance : InlineAlloc: " << inlineTimeSec << " msec vs STL: " << stdTimeSec
			   << " msec, rate = " << rate << lf;

			return rate;
		}
	};
} // namespace

void hbe::InlinePoolAllocatorTest::Prepare()
{
	AddTest("Zero-sized Allocation & Deallocation", [](auto&)
	{
		InlinePoolAllocator<int, 16> allocator;

		auto ptr = allocator.allocate(0);
		allocator.deallocate(ptr, 0);
	});

	auto AllocDeallocTestFunc = [](auto& allocator, auto& ls, auto& lf, int maxAllocSize) -> float
	{
		constexpr int testCount = 100000;

		ls << '[' << allocator.GetBlockSize() << "] Num Iterations = " << testCount
		   << ", maxAllocSize = " << maxAllocSize << lf;

		std::allocator<int> stdAlloc;

		Time::TDuration inlineTime;
		Time::TDuration stdTime;

		double inlineTimeDuration = 0;

		for (int j = 0; j < testCount; ++j)
		{
			{
				Time::ScopedTime measure(inlineTime);
				for (int i = 0; i < maxAllocSize; ++i)
				{
					auto ptr = allocator.allocate(i);
					allocator.deallocate(ptr, i);
				}
			}

			inlineTimeDuration += Time::ToFloat(inlineTime);
		}

		double stdTimeDuration = 0;
		for (int j = 0; j < testCount; ++j)
		{
			{
				Time::ScopedTime measure(stdTime);

				for (int i = 0; i < maxAllocSize; ++i)
				{
					auto ptr = stdAlloc.allocate(i);
					stdAlloc.deallocate(ptr, i);
				}
			}

			stdTimeDuration += Time::ToFloat(stdTime);
		}

		auto rate = static_cast<float>(inlineTimeDuration / stdTimeDuration);

#if PROFILE_ENABLED
		auto& mmgr = MemoryManager::GetInstance();
		auto stat = mmgr.GetAllocatorStat(allocator.GetID());

		ls << "Performance: " << inlineTimeAvg << " msec vs STL: " << stdTimeAvg << " msec, rate = [" << rate
		   << "], fallback count = " << stat.fallbackCount << " / " << (testCount * loopLength) << lf;
#else // PROFILE_ENABLED
		ls << "Performance: " << inlineTimeDuration << " msec vs STL: " << stdTimeDuration << " msec, rate = [" << rate
		   << ']' << lf;
#endif // PROFILE_ENABLED

		return rate;
	};

	AddTest("Perf. & Stress, Size(1)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 64;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 1);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(16)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 64;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 16);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(32)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 64;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 32);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(64)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 64;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 64);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(128)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 128;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 128);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(256)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 256;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 256);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(512)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 512;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 512);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(1024)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 1024;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 1024);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(2048)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 2048;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 2048);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Perf. & Stress, Size(8192)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 8192;
		InlinePoolAllocator<int, inlineSize> inlineAlloc;

		auto rate = AllocDeallocTestFunc(inlineAlloc, ls, lf, 8192);
		if (rate > 1.0f)
		{
			ls << "Performance is worse than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Vector Growth, size(32)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 64;
		ls << "==========================================" << lf;
		ls << "Inline Alloc Size = " << inlineSize << lf;

		auto rate = VectorGrowthTest<inlineSize>()(GetName(), ls, lf, 32);
		if (rate > 1.0f)
		{
			ls << "It's slower than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Vector Growth, size(128)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 128;
		ls << "==========================================" << lf;
		ls << "Inline Alloc Size = " << inlineSize << lf;

		auto rate = VectorGrowthTest<inlineSize>()(GetName(), ls, lf, 128);
		if (rate > 1.0f)
		{
			ls << "It's slower than system malloc. rate = " << rate << lfwarn;
		}
	});

	AddTest("Vector Growth, size(512)", [&, this](auto& ls)
	{
		constexpr int inlineSize = 512;
		ls << "==========================================" << lf;
		ls << "Inline Alloc Size = " << inlineSize << lf;

		auto rate = VectorGrowthTest<inlineSize>()(GetName(), ls, lf, 512);
		if (rate > 1.0f)
		{
			ls << "It's slower than system malloc. rate = " << rate << lfwarn;
		}
	});
}
#endif // __UNIT_TEST__
