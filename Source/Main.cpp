// Created by mooming.go@gmail.com, 2017

#include "Engine.h"
#include "Config/EngineConfig.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[])
{
#ifdef __UNIT_TEST__
    Test::RunUnitTests();
#endif // __UNIT_TEST__

    HE::Engine hengine;

    hengine.Initialize();
    hengine.Run();
    
    return 0;
}
