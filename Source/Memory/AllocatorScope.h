// Created by mooming.go@gmail.com, 2017

#pragma once

#include "AllocatorID.h"

#include <memory>


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
#include "Test/TestCase.h"

namespace HE
{
class AllocatorScopeTest : public TestCase
{
public:
    AllocatorScopeTest() : TestCase("AllocatorScopeTest") {}

protected:
    virtual bool DoTest() override;
};

} // HE

#endif //__UNIT_TEST__
