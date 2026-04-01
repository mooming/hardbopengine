// Created by mooming.go@gmail.com

#include "Engine/Engine.h"
#include "Test/UnitTestCollection.h"

#if VULKAN_SDK
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#else
// Vulkan SDK not available - test will run without Vulkan support
#endif

int main(int argc, const char* argv[])
{
    hbe::Engine hengine;
    hengine.Initialize(argc, argv);

	Test::RunTests();
    hengine.WaitForEnd();

    return 0;
}
