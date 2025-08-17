// Created by mooming.go@gmail.com 2022

#include "OSDebug.h"

#ifdef __UNIT_TEST__

namespace hbe
{

    void OSDebugTest::Prepare()
    {
        AddTest("Print CallStack", [this](auto& ls) {
            auto callstack = OS::GetBackTrace();
            ls << callstack.c_str() << lf;
        });
    }

} // namespace hbe
#endif //__UNIT_TEST__
