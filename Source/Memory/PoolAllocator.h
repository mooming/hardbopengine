// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "System/Types.h"


namespace HE
{
	class PoolAllocator final
	{
	public:
		using This = PoolAllocator;
        
	private:
		enum class IndexType : uint8_t
		{
			U32,
			U16,
			U8
		};
        
        TAllocatorID id;
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
        ~PoolAllocator();

        virtual Pointer Allocate(size_t size);
		virtual void Deallocate(const Pointer ptr);

        virtual size_t GetSize(const Pointer) const { return blockSize; }

		virtual size_t Usage() const
		{
			return (numberOfBlocks - numberOfFreeBlocks) * blockSize;
		}

		virtual size_t Available() const
		{
			return numberOfFreeBlocks * blockSize;
		}

		Pointer Allocate();
		inline Index NumberOfFreeBlocks()
		{
			return numberOfFreeBlocks;
		}
	};
} // HE

#ifdef __UNIT_TEST__

#include "Test/TestCase.h"


namespace HE
{
    class PoolAllocatorTest : public TestCase
    {
    public:
        PoolAllocatorTest() : TestCase("PoolAllocatorTest")
        {
        }

    protected:
        virtual bool DoTest() override;
    };
} // HE
#endif //__UNIT_TEST__
