// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "OSAL/Application.h"
#include "OSAL/Window.h"
#include "Renderer/IRenderer.h"
#include "Renderer/RHICapabilities.h"
#include "Renderer/RendererFactory.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <getopt.h>
#include <iostream>


using namespace hbe;
using namespace hbe::Renderer;

static std::atomic<bool> running{true};

void PrintUsage(const char* programName) noexcept {
	printf("Usage: %s [options]\n", programName);
	printf("Options:\n");
	printf("  -a, --api <api>     Specify rendering API (vulkan, metal, dx12)\n");
	printf("  -h, --help          Show this help message\n");
}

[[nodiscard]] APIType ParseAPIType(const char* apiString) noexcept {
	if (strcmp(apiString, "vulkan") == 0)
		return APIType::Vulkan;
	if (strcmp(apiString, "metal") == 0)
		return APIType::Metal;
	if (strcmp(apiString, "dx12") == 0)
		return APIType::DX12;
	return APIType::Unknown;
}

int main(int argc, char* argv[]) noexcept {
	std::signal(SIGINT, [](int) { running = false; });
	std::signal(SIGTERM, [](int) { running = false; });

	APIType preferredAPI = APIType::Vulkan;

	static struct option longOptions[] = {
		{"api", required_argument, 0, 'a'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	int optionIndex = 0;
	int c;

	while ((c = getopt_long(argc, argv, "a:h", longOptions, &optionIndex)) != -1) {
		switch (c) {
		case 'a':
			preferredAPI = ParseAPIType(optarg);
			break;
		case 'h':
			PrintUsage(argv[0]);
			return 0;
		default:
			PrintUsage(argv[0]);
			return 1;
		}
	}

	Engine hengine;
	hengine.Initialize(argc, (const char**)argv);

	auto app = OS::CreateApplication();
	if (!app) {
		std::cerr << "Error: Failed to create application" << std::endl;
		return 1;
	}

	app->Initialize();

	auto window = OS::CreateWindow("VulkanExample - Rotating Quad", 800, 600);
	if (!window) {
		std::cerr << "Error: Failed to create window" << std::endl;
		return 1;
	}

	window->SetVisible(true);

	const char* apiName = "Unknown";
	switch (preferredAPI) {
	case APIType::Vulkan: apiName = "Vulkan"; break;
	case APIType::Metal: apiName = "Metal"; break;
	case APIType::DX12: apiName = "DX12"; break;
	default: break;
	}

	window->SetTitle(HString("VulkanExample - Rotating Quad (") + apiName + ")");

	auto renderer = RendererFactory::Create(preferredAPI);
	if (!renderer) {
		printf("Failed to create renderer for API: %s\n", apiName);
		std::cerr << "Error: Failed to create renderer for API: " << apiName << std::endl;
		return 1;
	}

	renderer->Initialize(window.get());

	printf("Rendering with: %s\n", apiName);

	while (running && !window->IsClosed()) {
		app->PollEvents();

		static auto lastTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		renderer->BeginFrame();
		renderer->Render(deltaTime);
		renderer->EndFrame();

		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	renderer->Shutdown();
	window->Close();
	hengine.ShutDown();

	return 0;
}
