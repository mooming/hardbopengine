// Copyright (c) 2026 HardBop Engine. All rights reserved.
// TriangleExample - minimal DOD triangle demo using OSAL window

#include "Engine/Engine.h"
#include "Engine/OSAL/Application.h"
#include "Engine/OSAL/Window.h"
#include "DODTypes.h"
#include "SoftwareRenderer.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>


using namespace hbe;

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

	const int windowWidth = 640;
	const int windowHeight = 480;
	auto window = OS::CreateWindow("Triangle Example", windowWidth, windowHeight);
	if (!window)
	{
		std::cerr << "Failed to create window" << std::endl;
		return 1;
	}

	// Instantiate the DOD Software Renderer
	TriangleExample::SoftwareRenderer renderer;
	renderer.Initialize(window.get(), windowWidth, windowHeight);

	// Pipeline Data
	TriangleExample::InstanceData instanceData;
	std::vector<TriangleExample::DrawCommand> drawCommands;

	auto startTime = std::chrono::steady_clock::now();

	// Main loop – render until user closes window or signals
	while (!window->IsClosed())
	{
		app->PollEvents();
		window->PollEvents();

		// Calculate delta time / elapsed time
		auto currentTime = std::chrono::steady_clock::now();
		float elapsed = std::chrono::duration<float>(currentTime - startTime).count();

		// ---------------------------------------------------------------------------
		// PHASE 1: EXTRACT
		// Update instance data (SoA) from "game state"
		// ---------------------------------------------------------------------------
		instanceData.Clear();
		
		// We'll render 3 rotating triangles
		for (int i = 0; i < 3; ++i) {
			float angle = elapsed + (i * 2.0f);
			Math::TFloat4x4 transform = Math::TFloat4x4::RotationY(angle);
			transform = transform * Math::TFloat4x4::Translation({ (i - 1) * 0.6f, 0.0f, 0.0f });
			
			instanceData.worldMatrices.push_back(transform);
			instanceData.colors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });
		}

		// ---------------------------------------------------------------------------
		// PHASE 2: PREPARE
		// Generate and sort draw commands
		// ---------------------------------------------------------------------------
		drawCommands.clear();
		for (uint32_t i = 0; i < static_cast<uint32_t>(instanceData.Size()); ++i) {
			TriangleExample::DrawCommand cmd;
			cmd.meshHandle = i;
			cmd.indexCount = 3;
			cmd.instanceCount = 1;
			cmd.firstIndex = 0;
			cmd.vertexOffset = 0;
			cmd.firstInstance = 0;
			// Simple sort key: just use index for now since we have 1 PSO/Material
			cmd.sortKey = TriangleExample::RenderSortKey::Make(0, 0, 0, i);
			drawCommands.push_back(cmd);
		}
		
		// Sort commands by key to minimize state changes (Crucial for GPU backends)
		std::sort(drawCommands.begin(), drawCommands.end(), [](const auto& a, const auto& b) {
			return a.sortKey < b.sortKey;
		});

		// ---------------------------------------------------------------------------
		// PHASE 3: SUBMIT
		// Pass prepared data to the renderer backend
		// ---------------------------------------------------------------------------
		renderer.Render(instanceData, drawCommands);

		// Simple frame limiting
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	window->Close();
	engine.ShutDown();

	return 0;
}
