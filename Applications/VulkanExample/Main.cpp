// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "Math/MathUtil.h"
#include "Log/Logger.h"
#include "OSAL/Application.h"
#include "OSAL/Window.h"
#include "Renderer/Vulkan/VulkanRenderer.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>


using namespace hbe;
using namespace hbe::Renderer;

namespace
{
std::atomic<bool> running{true};

// All matrices are column-major (element [col * 4 + row]), matching the GLSL mat4
// layout that VulkanRenderer pushes to the shader.
void SetIdentity(float m[16]) noexcept
{
	std::memset(m, 0, sizeof(float) * 16);
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void SetTranslation(float m[16], float x, float y, float z) noexcept
{
	SetIdentity(m);
	m[12] = x;
	m[13] = y;
	m[14] = z;
}

void SetRotationY(float m[16], float radians) noexcept
{
	SetIdentity(m);
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	m[0] = c;	 m[2] = -s;
	m[8] = s;	 m[10] = c;
}

/// @brief Right-handed perspective mapped into Vulkan's [0, 1] depth and Y-down framebuffer.
void SetPerspective(float m[16], float fovRadians, float aspect, float nearZ, float farZ) noexcept
{
	std::memset(m, 0, sizeof(float) * 16);
	const float f = 1.0f / std::tan(fovRadians * 0.5f);
	m[0] = f / aspect;
	m[5] = -f; // Y flip: world +Y is screen up
	m[10] = farZ / (nearZ - farZ);
	m[11] = -1.0f;
	m[14] = farZ * nearZ / (nearZ - farZ);
}

/// @brief A unit-ish quad in the XY plane facing +Z, so the fixed directional light
///        sweeps across it as it rotates about Y.
Mesh MakeQuad() noexcept
{
	Mesh mesh;
	mesh.vertices = {
		MeshVertex(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
		MeshVertex( 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
		MeshVertex( 1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
		MeshVertex(-1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
	};
	mesh.indices = {0, 1, 2, 0, 2, 3};
	return mesh;
}
} // namespace

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
		// The logger is asynchronous: flush so its diagnostics survive this exit.
		Logger::Get().Flush();
		std::cerr << "Error: Failed to initialize Vulkan renderer" << std::endl;
		return 1;
	}

	const Mesh quad = MakeQuad();
	renderer.SetMesh(quad);

	printf("Rendering with: Vulkan\n");

	// The drawable follows the window content rect, so the aspect ratio comes from the
	// renderer rather than the 800x600 we asked the window for.
	const VkExtent2D extent = renderer.GetExtent();

	float view[16];
	float proj[16];
	float model[16];
	SetPerspective(proj, DegreeToRadian(60.0f), static_cast<float>(extent.width) / static_cast<float>(extent.height),
		0.1f, 100.0f);
	SetTranslation(view, 0.0f, 0.0f, -3.0f);
	renderer.SetView(view);
	renderer.SetProj(proj);

	float angle = 0.0f;

	while (running && !window->IsClosed()) {
		app->PollEvents();

		static auto lastTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		angle += deltaTime * 1.2f;
		if (angle > 2.0f * Pi) angle -= 2.0f * Pi;

		SetRotationY(model, angle);
		renderer.SetModel(model);

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
