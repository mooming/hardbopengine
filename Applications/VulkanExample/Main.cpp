// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "OSAL/Application.h"
#include "OSAL/Window.h"
#include "Renderer/IRenderer.h"
#include "Renderer/RHICapabilities.h"
#include "Renderer/RendererFactory.h"

#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using namespace hbe;
using namespace hbe::Renderer;

static std::atomic<bool> g_Running{true};

int main(int argc, const char* argv[])
{
	std::signal(SIGINT, [](int) { g_Running = false; });
	std::signal(SIGTERM, [](int) { g_Running = false; });

	Engine hengine;
	hengine.Initialize(argc, argv);

	auto app = OS::CreateApplication();
	if (!app)
	{
		return 1;
	}

	app->Initialize();

	auto window = OS::CreateWindow("VulkanExample - Rotating Quad", 800, 600);
	if (!window)
	{
		return 1;
	}

	window->SetVisible(true);

	auto apiType = RHICapabilities::GetPreferredAPI();
	const char* apiName = "Unknown";
	switch (apiType)
	{
	case APIType::Vulkan: apiName = "Vulkan"; break;
	case APIType::Metal: apiName = "Metal"; break;
	case APIType::DX12: apiName = "DX12"; break;
	default: break;
	}

	window->SetTitle(HString("VulkanExample - Rotating Quad (") + apiName + ")");

	auto renderer = RendererFactory::CreateWithFallback();
	if (!renderer)
	{
		return 1;
	}

	renderer->Initialize(window.get());

	while (g_Running && !window->IsClosed())
	{
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