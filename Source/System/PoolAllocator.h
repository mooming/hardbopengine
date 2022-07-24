// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef PoolAllocator_h
#define PoolAllocator_h

#include "Allocator.h"

namespace HE
{
	class PoolAllocator : public Allocator
	{
	public:
		using This = PoolAllocator;

	private:
		enum IndexType
		{
			U32,
			U16,
			U8
		};

		Index blockSize;
		Index numberOfBlocks;
		Index numberOfFreeBlocks;
		Byte* buffer;
		Pointer availables;
		IndexType indexType;

		Index GetIndex(Pointer ptr);
		void SetIndex(Pointer ptr, Index index);

	public:
		PoolAllocator(Index blockSize, Index numberOfBlocks);
		virtual ~PoolAllocator();

        virtual Pointer Allocate(size_t size) override;
		virtual void Deallocate(const Pointer ptr) override;

        virtual size_t GetSize(const Pointer) const override { return blockSize; }

		virtual size_t Usage() const override
		{
			return (numberOfBlocks - numberOfFreeBlocks) * blockSize;
		}

		virtual size_t Available() const override
		{
			return numberOfFreeBlocks * blockSize;
		}

		Pointer Allocate();
		inline Index NumberOfFreeBlocks()
		{
			return numberOfFreeBlocks;
		}
	};

#ifdef __UNIT_TEST__
    class PoolAllocatorTest : public TestCase
    {
    public:
        PoolAllocatorTest() : TestCase("PoolAllocatorTest") {}

    protected:
        virtual bool DoTest() override;
    };
#endif //__UNIT_TEST__
}

#endif /* PoolAllocator_h */
