// Created by mooming.go@gmail.com

#include "Engine/Engine.h"
#include "Test/UnitTestCollection.h"

int main(int argc, const char* argv[])
{
    hbe::Engine hengine;
    hengine.Initialize(argc, argv);

	Test::RunTests();
    hengine.WaitForEnd();

    return 0;
}
