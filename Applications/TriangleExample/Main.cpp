// TriangleExample - minimal Vulkan triangle demo using OSAL window

#include "Engine/Engine.h"
#include "Engine/OSAL/Application.h"
#include "Engine/OSAL/Window.h"
#include "MinimalVulkanRenderer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>


using namespace hbe;
static std::atomic<bool> running{true};

void signalHandler(int)
{
	running = false;
}

int main(int argc, const char* argv[]) noexcept
{
	// Initialize engine (required for OSAL)
	Engine engine;
	engine.Initialize(argc, argv);

	// Create application and window
	auto app = OS::CreateApplication();
	if (!app)
	{
		std::cerr << "Failed to create application" << std::endl;
		return 1;
	}

	app->Initialize();

	auto window = OS::CreateWindow("Triangle Example", 640, 480);
	if (!window)
	{
		std::cerr << "Failed to create window" << std::endl;
		return 1;
	}

	// Register signal handler for graceful exit
	std::signal(SIGINT, signalHandler);
	std::signal(SIGTERM, signalHandler);

	MinimalVulkanRenderer renderer;
	if (!renderer.Initialize(window.get()))
	{
		std::cerr << "Failed to initialize renderer" << std::endl;
		return 1;
	}

	// Main loop – render until user closes window or signals
	while (running && !window->IsClosed())
	{
		app->PollEvents(); // process OS events

		renderer.BeginFrame();
		renderer.Render(0.016f); // simulate ~60 FPS
		renderer.EndFrame();
		// Simple frame limiting
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	renderer.Shutdown();
	window->Close();
	engine.ShutDown();

	return 0;
}
