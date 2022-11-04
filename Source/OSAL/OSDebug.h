// Created by mooming.go@gmail.com 2022

#pragma once

#include "String/StaticString.h"


namespace OS
{

HE::StaticString GetBackTrace();

} // OS

#ifdef __UNIT_TEST__
#include "Test/TestCollection.h"

namespace HE
{

class OSDebugTest : public TestCollection
{
public:
    inline OSDebugTest() : TestCollection("OSDebugTest")
    {
    }

protected:
    virtual void Prepare() override;
};

} // HE
#endif //__UNIT_TEST__
