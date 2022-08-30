// Created by mooming.go@gmail.com, 2017

#include "MathUtil.h"

#include "Config/EngineConfig.h"


#ifdef __UNIT_TEST__
#include "Log/Logger.h"


bool HE::MathUtilTest::DoTest()
{
    using namespace std;

    auto log = Logger::Get(GetName());

    {

        log.Out([](auto& ls)
        {
            ls << "2^10 = " << Pow(2, 10);
        });

        if (Pow(2, 10) != 1024)
        {
            log.OutError([](auto& ls)
            {
                ls << "Pow(int, int) result failed = " << Pow(2, 10)
                    << ", but 1024 expected.";
            });

            return false;
        }

        log.Out([](auto& ls)
        {
            ls << "2.0f^10.0f = " << Pow(2.0f, 10.0f);
        });
        
        if (Pow(2, 10) != Pow(2.0f, 10.0f))
        {
            log.OutError([](auto& ls)
            {
                ls << "Pow(float, float) result failed = " << Pow(2.0f, 10.0f)
                    << ", but 1024.0f expected.";
            });

            return false;
        }
    }

    return true;
}

#endif //__UNIT_TEST__
