// Copyright (c) 2026 HardBop Engine. All rights reserved.
// TriangleExample - minimal DOD triangle demo using OSAL window

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include "DODTypes.h"
#include "Engine/Core/CommandLineArguments.h"
#include "Engine/Engine.h"
#include "Engine/OSAL/Application.h"
#include "Engine/OSAL/Window.h"
#include "SoftwareRenderer.h"


using namespace hbe;

int main(int argc, const char* argv[]) noexcept
{
	// Initialize engine (required for OSAL)
	Engine engine;
	engine.Initialize(argc, argv);

	// Parse optional command‑line flags (only headless supported for now)
	hbe::CommandLineArguments cmdArgs(argc, argv);

	int maxFps = 60; // default
	bool headless = false;

	auto& arguments = cmdArgs.GetArguments();
	headless = arguments.Contains("--headless");

	// Create application and window (skip if headless)
	auto app = OS::CreateApplication();
	if (!app)
	{
		std::cerr << "Failed to create application" << std::endl;
		return 1;
	}

	app->Initialize();

	const int windowWidth = 640;
	const int windowHeight = 480;
	std::unique_ptr<OS::IWindow> window;

	if (!headless)
	{
		window = OS::CreateWindow("Triangle Example", windowWidth, windowHeight);
		if (!window)
		{
			std::cerr << "Failed to create window" << std::endl;
			return 1;
		}
	}

	// Instantiate the DOD Software Renderer
	TriangleExample::SoftwareRenderer renderer;
	renderer.Initialize(headless ? nullptr : window.get(), windowWidth, windowHeight);

	// Pipeline Data
	TriangleExample::InstanceData instanceData;
	std::vector<TriangleExample::DrawCommand> drawCommands;

	auto startTime = std::chrono::steady_clock::now();

	// Main loop – render until user closes window or signals
	while (!headless || !window->IsClosed())
	{
		app->PollEvents();

		if (!headless)
		{
			window->PollEvents();
		}

		// Calculate delta time / elapsed time
		auto currentTime = std::chrono::steady_clock::now();
		float elapsed = std::chrono::duration<float>(currentTime - startTime).count();

		// ---------------------------------------------------------------------------
		// PHASE 1: EXTRACT
		// Update instance data (SoA) from "game state"
		// ---------------------------------------------------------------------------
		instanceData.Clear();

		// We'll render 3 rotating triangles
		for (int i = 0; i < 3; ++i)
		{
			float angle = elapsed + (i * 2.0f);
			Math::TFloat4x4 transform;
			transform.SetRotationY(angle);
			transform.SetTranslation(Math::TFloat3{(i - 1) * 0.6f, 0.0f, 0.0f});

			instanceData.worldMatrices.push_back(transform);
			instanceData.colors.push_back({1.0f, 1.0f, 1.0f, 1.0f});
		}

		// ---------------------------------------------------------------------------
		// PHASE 2: PREPARE
		// Project vertices and generate prepared buffers for the renderer
		// ---------------------------------------------------------------------------
		TriangleExample::PrepareBuffers prepareBuffers;

		for (uint32_t i = 0; i < static_cast<uint32_t>(instanceData.Size()); ++i)
		{
			const Math::TFloat4x4& world = instanceData.worldMatrices[i];
			const Math::TFloat4& color = instanceData.colors[i];

			TriangleExample::ProjectedTriangle tri;
			tri.color = color;

			Math::TFloat3 screenPos[3];
			for (int v = 0; v < 3; ++v)
			{
				Math::TFloat4 worldPos = world * Math::TFloat4(TriangleExample::TriangleMesh[v].position, 1.0f);
				// Simple orthographic projection
				screenPos[v].x = (worldPos.x + 1.0f) * 0.5f * windowWidth;
				screenPos[v].y = (1.0f - (worldPos.y + 1.0f) * 0.5f) * windowHeight;
				screenPos[v].z = worldPos.z;
			}

			tri.v0 = screenPos[0];
			tri.v1 = screenPos[1];
			tri.v2 = screenPos[2];

			// Precompute inverse area for barycentric coordinates
			float area = (tri.v1.x - tri.v0.x) * (tri.v2.y - tri.v0.y) - (tri.v1.y - tri.v0.y) * (tri.v2.x - tri.v0.x);

			tri.invArea = (std::abs(area) > 1e-6f) ? (1.0f / area) : 0.0f;

			prepareBuffers.triangles.push_back(tri);
		}

		// ---------------------------------------------------------------------------
		// PHASE 3: SUBMIT
		// Pass prepared data to the renderer backend
		// ---------------------------------------------------------------------------
		renderer.Render(prepareBuffers);

		// Break after 3 seconds in headless mode
		if (headless && elapsed >= 3.0f)
		{
			break;
		}

		// Simple frame limiting based on maxFps
		auto frameEnd = std::chrono::steady_clock::now();
		auto frameTime = std::chrono::duration<float>(frameEnd - startTime);
		float targetFrameSec = 1.0f / static_cast<float>(maxFps);
		if (frameTime.count() < targetFrameSec)
		{
			std::this_thread::sleep_for(std::chrono::duration<float>(targetFrameSec - frameTime.count()));
		}

		// Write a 16x16 PPM snapshot each second
		static float lastSnapshot = 0.0f;
		if (elapsed - lastSnapshot >= 1.0f)
		{
			lastSnapshot = elapsed;
			std::string filename = "triangle_snapshot_" + std::to_string(static_cast<int>(elapsed)) + ".ppm";
			std::ofstream out(filename, std::ios::binary);
			if (out)
			{
				int snapW = std::min(16, windowWidth);
				int snapH = std::min(16, windowHeight);
				out << "P6\n" << snapW << " " << snapH << "\n255\n";
				for (int y = 0; y < snapH; ++y)
				{
					for (int x = 0; x < snapW; ++x)
					{
						uint32_t pixel = renderer.frameBuffer[y * windowWidth + x];
						unsigned char r = (pixel >> 0) & 0xFF;
						unsigned char g = (pixel >> 8) & 0xFF;
						unsigned char b = (pixel >> 16) & 0xFF;
						out.write(reinterpret_cast<char*>(&r), 1);
						out.write(reinterpret_cast<char*>(&g), 1);
						out.write(reinterpret_cast<char*>(&b), 1);
					}
				}
				out.close();
			}
		}
	}

	if (window)
	{
		window->Close();
	}

	engine.ShutDown();

	return 0;
}
