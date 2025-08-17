// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "Config/BuildConfig.h"
#include "OSAL/SourceLocation.h"
#include "System/Types.h"

namespace hbe
{
    class StackAllocator final
    {
    public:
        using This = StackAllocator;
        using SizeType = size_t;

#ifdef PROFILE_ENABLED
        using TSrcLoc = std::source_location;
#endif // PROFILE_ENABLED

    private:
        TAllocatorID id;
        TAllocatorID parentID;

        SizeType capacity;
        SizeType cursor;

        union
        {
            Byte* buffer;
            Pointer bufferPtr;
        };

#ifdef PROFILE_ENABLED
        TSrcLoc srcLocation;
#endif // PROFILE_ENABLED

    public:
#ifdef PROFILE_ENABLED
        StackAllocator(const char* name, SizeType capacity,
            const TSrcLoc location = TSrcLoc::current());
#else  // PROFILE_ENABLED
        StackAllocator(const char* name, SizeType capacity);
#endif // PROFILE_ENABLED

        ~StackAllocator();

        Pointer Allocate(size_t size);
        void Deallocate(const Pointer ptr, SizeType size);

        size_t GetAvailable() const;
        size_t GetUsage() const;

        inline auto GetID() const { return id; }
        inline size_t GetSize(const Pointer) const { return 0; }

    private:
        bool IsMine(Pointer ptr) const;
    };
} // namespace hbe

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace hbe
{

    class StackAllocatorTest : public TestCollection
    {
    public:
        StackAllocatorTest()
            : TestCollection("StackAllocatorTest")
        {
        }

    protected:
        virtual void Prepare() override;
    };

} // namespace hbe
#endif //__UNIT_TEST__
