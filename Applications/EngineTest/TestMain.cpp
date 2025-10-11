// Created by mooming.go@gmail.com

#include "Engine/Engine.h"
#include "Test/UnitTestCollection.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main(int argc, const char* argv[])
{
	// if (!glfwInit())
	// {
	// 	return -1;
	// }
	//
	// auto result = glfwVulkanSupported();
	// if (result != GLFW_TRUE)
	// {
	// 	// Vulkan is available, at least for compute
	// 	return -1;
	// }

    hbe::Engine hengine;

    hengine.Initialize(argc, argv);

	Test::RunTests();

    hengine.WaitForEnd();

    return 0;
}
