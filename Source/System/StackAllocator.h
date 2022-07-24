// Copyright Hansol Park (anav96@naver.com, mooming.go@gmail.com). All rights reserved.

#ifndef StackAllocator_h
#define StackAllocator_h

#include "Allocator.h"

namespace HE
{
	class StackAllocator : public Allocator
	{
	public:
		using This = StackAllocator;
        using SizeType = size_t;

    private:
        SizeType capacity;
        SizeType freeSize;
        SizeType cursor;
        
        union
        {
            Byte* buffer;
            Pointer bufferPtr;
        };

	public:
		StackAllocator(size_t capacity, AllocatorId allocatorId = NATIVE_ALLOCATOR_ID);
		virtual ~StackAllocator();

		virtual Pointer Allocate(size_t size) override;
		virtual void Deallocate(const Pointer ptr) override;

        virtual inline size_t GetSize(const Pointer) const override { return 0; }
		virtual size_t Usage() const override;
		virtual size_t Available() const override;

    private:
        bool IsMine(Pointer ptr) const;
	};
}

#ifdef __UNIT_TEST__

#include "TestCase.h"

namespace HE
{
    class StackAllocatorTest : public TestCase
    {
    public:
        StackAllocatorTest() : TestCase("StackAllocatorTest") {}

    protected:
        virtual bool DoTest() override;
    };
}
#endif //__UNIT_TEST__


#endif /* StackAllocator_h */
