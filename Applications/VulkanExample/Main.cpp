// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "OSAL/Application.h"
#include "OSAL/Window.h"
#include "Renderer/Vulkan/VulkanRenderer.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>


using namespace hbe;
using namespace hbe::Renderer;

static std::atomic<bool> running{true};

int main(int argc, char* argv[]) noexcept {
	std::signal(SIGINT, [](int) { running = false; });
	std::signal(SIGTERM, [](int) { running = false; });

	Engine hengine;
	hengine.Initialize(argc, (const char**)argv);

	auto app = OS::CreateApplication();
	if (!app) {
		std::cerr << "Error: Failed to create application" << std::endl;
		return 1;
	}

	app->Initialize();

	auto window = OS::CreateWindow("VulkanExample - Rotating Quad (Vulkan)", 800, 600);
	if (!window) {
		std::cerr << "Error: Failed to create window" << std::endl;
		return 1;
	}

	window->SetVisible(true);

	// Vulkan-only: construct the concrete renderer directly (no factory, no inheritance).
	VulkanRenderer renderer;
	if (!renderer.Initialize(window.get())) {
		std::cerr << "Error: Failed to initialize Vulkan renderer" << std::endl;
		return 1;
	}

	printf("Rendering with: Vulkan\n");

	while (running && !window->IsClosed()) {
		app->PollEvents();

		static auto lastTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		renderer.BeginFrame();
		renderer.Render(deltaTime);
		renderer.EndFrame();

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	renderer.Shutdown();
	window->Close();
	hengine.ShutDown();

	return 0;
}
