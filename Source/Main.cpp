// Created by mooming.go@gmail.com, 2017

#include "Engine.h"
#include "Config/BuildConfig.h"
#include "Test/UnitTestCollection.h"


int main(int argc, const char* argv[])
{
    HE::Engine hengine;

    hengine.Initialize(argc, argv);
    
#ifdef __UNIT_TEST__
    Test::RunUnitTests();
#endif // __UNIT_TEST__

    hengine.Run();
    
    return 0;
}
