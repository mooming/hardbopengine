// Created by mooming.go@gmail.com

#include "Engine/Engine.h"


int main(int argc, const char* argv[])
{
    HE::Engine hengine;

    hengine.Initialize(argc, argv);
    hengine.Run();

    return 0;
}
