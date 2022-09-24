// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"


namespace HE
{

class AllocatorScope final
{
private:
    TAllocatorID previous;
    TAllocatorID current;

public:
    AllocatorScope(const AllocatorScope&) = delete;
    AllocatorScope(AllocatorScope&&) = delete;
    AllocatorScope& operator= (const AllocatorScope&) = delete;
    AllocatorScope& operator= (AllocatorScope&&) = delete;

public:
    AllocatorScope();
    AllocatorScope(TAllocatorID id);
    
    template <typename T>
    AllocatorScope(const T& allocator)
        : AllocatorScope(allocator.GetID())
    {
    }
    
    ~AllocatorScope();
};

} // HE

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"


namespace HE
{

class AllocatorScopeTest
    : public TestCollection
{
public:
    AllocatorScopeTest()
        : TestCollection("AllocatorScopeTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // HE

#endif //__UNIT_TEST__
