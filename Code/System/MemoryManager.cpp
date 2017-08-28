// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#include "System/MemoryManager.h"

#include "System/Allocator.h"
#include "System/Exception.h"
#include "System/StdUtil.h"

using namespace HE;

namespace
{
	constexpr auto SizeTypeSize = sizeof(size_t);
	thread_local AllocatorId allocatorId = 1;

	class NullAllocator : public Allocator
	{
	public:
		NullAllocator() : Allocator(0)
		{
		}

		virtual Pointer Allocate(size_t size) override
		{
			throw Exception(__FILE__, __LINE__, "Null Allocation Error");

			return nullptr;
		}

		virtual void Deallocate(const Pointer ptr) override
		{
			throw Exception(__FILE__, __LINE__, "Null De-Allocation Error");
		}

        virtual size_t GetSize(const Pointer ptr) const override
        {
            return 0;
        }

		virtual size_t Usage() const override
		{
			return 0;
		}

		virtual size_t Available() const override
		{
			return 0;
		}
	};


	class NativeAllocator : public Allocator
	{
	private:
		size_t usage;
		size_t maxSize;

	public:
		NativeAllocator(const size_t maxSize) : Allocator(1), usage(0), maxSize(maxSize)
		{
		}

		virtual Pointer Allocate(size_t size) override
		{
			size_t effectiveSize = size + SizeTypeSize;
			usage += effectiveSize;

			if (usage > maxSize)
			{
				usage -= effectiveSize;
				throw Exception(__FILE__, __LINE__
					, "Native Allocator : Allocation Error, required = %u, usage = %u, max = %u"
					, size, usage, maxSize);
			}

			Byte* ptr = new Byte[effectiveSize];
			SetAs<size_t>(ptr, effectiveSize);
			ptr += sizeof(size_t);

			return ptr;
		}

		virtual void Deallocate(const Pointer ptr) override
		{
			Byte* bytePtr = static_cast<Byte*>(ptr);

			bytePtr -= SizeTypeSize;
			size_t size = GetAs<size_t>(bytePtr);
			usage -= size;

			delete[] bytePtr;
		}

        virtual size_t GetSize(const Pointer ptr) const override
        {
            Byte* bytePtr = static_cast<Byte*>(ptr);
            bytePtr -= SizeTypeSize;

            return GetAs<size_t>(bytePtr);
        }

		virtual size_t Usage() const override
		{
			return usage;
		}

		virtual size_t Available() const override
		{
			return maxSize - usage;
		}
	};
} // unnamed namespace

MemoryManager::MemoryManager() : allocators{}, freeId(2)
{
	static NullAllocator nullAllocator;
	static NativeAllocator nativeAllocator(1024 * 1024 * 1024);

	allocators[0] = &nullAllocator;
	allocators[1] = &nativeAllocator;

    SetCurrent(1);

    const int length = sizeof(allocators) / sizeof(allocators[0]);
	for (int i = 2; i < length; ++i)
	{
		allocators[i] = reinterpret_cast<Allocator*>(i + 1);
	}
}

void MemoryManager::SetAllocator(AllocatorId id)
{
	allocatorId = id;
}

AllocatorId MemoryManager::GetAllocator()
{
	return allocatorId;
}

size_t MemoryManager::TotalUsage()
{
	return allocators[1]->Usage();
}
