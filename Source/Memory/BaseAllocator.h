// Created by mooming.go@gmail.com

#pragma once

#include "AllocatorID.h"
#include "AllocatorScope.h"
#include "MemoryManager.h"
#include <cstddef>

namespace HE
{

template <typename T>
class BaseAllocator
{
public:
    using value_type = T;

    template <class U>
    struct rebind
    {
        using other = BaseAllocator<U>;
    };

private:
    TAllocatorID allocatorID;

public:
    BaseAllocator()
        : allocatorID(MemoryManager::GetCurrentAllocatorID())
    {
    }

    template <class U>
    BaseAllocator(const BaseAllocator<U>& rhs)
        : allocatorID(rhs.GetSourceAllocatorID())
    {
    }

    virtual ~BaseAllocator() = default;

    T* allocate(std::size_t n)
    {
        AllocatorScope scope(allocatorID);
        auto& mmgr = MemoryManager::GetInstance();
        auto ptr = mmgr.AllocateTypes<T>(n);

        return ptr;
    }

    void deallocate(T* ptr, std::size_t n)
    {
        AllocatorScope scope(allocatorID);
        auto& mmgr = MemoryManager::GetInstance();
        mmgr.DeallocateTypes(ptr, n);
    }

    template <class U>
    bool operator==(const BaseAllocator<U>& rhs) const
    {
        return allocatorID == rhs.allocatorID;
    }

    template <class U>
    bool operator!=(const BaseAllocator<U>& rhs) const
    {
        return allocatorID != rhs.allocatorID;
    }

    auto GetID() const { return allocatorID; }
    auto GetSourceAllocatorID() const { return allocatorID; }
    size_t GetFallbackCount() const { return 0; }
};

} // namespace HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class BaseAllocatorTest : public TestCollection
{
public:
    BaseAllocatorTest()
        : TestCollection("BaseAllocatorTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // namespace HE
#endif //__UNIT_TEST__
