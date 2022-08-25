// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"
#include "String/StaticString.h"
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
        StaticString name;
        
        Index blockSize;
		Index numberOfBlocks;
		Index numberOfFreeBlocks;

        Byte* buffer;
		Pointer availables;
		IndexType indexType;

	public:
		PoolAllocator(const char* name, Index blockSize, Index numberOfBlocks);
        ~PoolAllocator();

        Pointer Allocate(size_t size);
		void Deallocate(Pointer ptr);

        inline auto GetID() const { return id; }
        inline auto GetName() const { return name; }
        inline size_t GetSize(const Pointer) const { return blockSize; }
        inline  size_t Usage() const { return (numberOfBlocks - numberOfFreeBlocks) * blockSize; }
        inline size_t Available() const { return numberOfFreeBlocks * blockSize; }
        inline Index NumberOfFreeBlocks() const { return numberOfFreeBlocks; }
        
    private:
        Index GetIndex(Pointer ptr) const;
        Index ReadNextIndex(Pointer ptr) const;
        void WriteNextIndex(Pointer ptr, Index index);
        Pointer Allocate();
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
