// Created by mooming.go@gmail.com, 2017

#pragma once

#include "Allocator.h"
#include "Config/EngineConfig.h"


namespace HE
{
	template <typename T>
	class HardbopAllocator final
	{
	public:
		typedef T value_type;

	public:
		HardbopAllocator() = default;

		template<typename U>
		HardbopAllocator(const HardbopAllocator<U>& rhs)
		{
		}

		T* allocate(std::size_t n)
		{
#ifdef __USE_SYSTEM_MALLOC__
			return (T*)malloc(sizeof(T) * n);
#else // __USE_SYSTEM_MALLOC__

			auto ptr = Allocate<T>(n);
			if (ptr == nullptr)
				throw std::bad_alloc();

			return ptr;
#endif // __USE_SYSTEM_MALLOC__
		}

		void deallocate(T* ptr, std::size_t) noexcept
		{
#ifdef __USE_SYSTEM_MALLOC__
			free(ptr);
#else // __USE_SYSTEM_MALLOC__
			Deallocate(ptr);
#endif // __USE_SYSTEM_MALLOC__	
		}
	};

	template <class T, class U>
	bool operator==(const HardbopAllocator<T>&, const HardbopAllocator<U>&)
	{
		return true;
	}

	template <class T, class U>
	bool operator!=(const HardbopAllocator<T>&, const HardbopAllocator<U>&)
	{
		return false;
	}
} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCase.h"

namespace HE
{
	class HardbopAllocatorTest : public TestCase
	{
	public:

		HardbopAllocatorTest() : TestCase("HardbopAllocatorTest")
		{
		}

	protected:
		virtual bool DoTest() override;
	};
} // HE
#endif //__UNIT_TEST__
