// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

#include "Engine/Engine.h"
#include "Log/Logger.h"
#include "Math/MathUtil.h"
#include "OSAL/Application.h"
#include "OSAL/Window.h"
#include "Renderer/Vulkan/VulkanRenderer.h"


using namespace hbe;
using namespace hbe::Renderer;

namespace
{
std::atomic<bool> running{true};

constexpr size_t MatrixElementCount = 16;

constexpr int RequestedWindowWidth = 800;
constexpr int RequestedWindowHeight = 600;

constexpr float FieldOfViewDegrees = 60.0f;
constexpr float NearZ = 0.1f;
constexpr float FarZ = 100.0f;
constexpr float CameraZ = -3.0f;

constexpr float RadiansPerSecond = 1.2f;
constexpr auto FrameSleep = std::chrono::milliseconds(16);

// All matrices are column-major (element [col * 4 + row]), matching the GLSL mat4
// layout that VulkanRenderer pushes to the shader.
void SetIdentity(float outMatrix[MatrixElementCount]) noexcept
{
	std::memset(outMatrix, 0, sizeof(float) * MatrixElementCount);
	outMatrix[0] = outMatrix[5] = outMatrix[10] = outMatrix[15] = 1.0f;
}

void SetTranslation(float outMatrix[MatrixElementCount], float x, float y, float z) noexcept
{
	SetIdentity(outMatrix);
	outMatrix[12] = x;
	outMatrix[13] = y;
	outMatrix[14] = z;
}

void SetRotationY(float outMatrix[MatrixElementCount], float radians) noexcept
{
	SetIdentity(outMatrix);
	const float cosAngle = std::cos(radians);
	const float sinAngle = std::sin(radians);
	outMatrix[0] = cosAngle;
	outMatrix[2] = -sinAngle;
	outMatrix[8] = sinAngle;
	outMatrix[10] = cosAngle;
}

/// @brief Right-handed perspective mapped into Vulkan's [0, 1] depth and Y-down framebuffer.
void SetPerspective(float outMatrix[MatrixElementCount], float fovRadians, float aspect, float nearZ,
					float farZ) noexcept
{
	std::memset(outMatrix, 0, sizeof(float) * MatrixElementCount);
	const float inverseHalfFovTangent = 1.0f / std::tan(fovRadians * 0.5f);
	outMatrix[0] = inverseHalfFovTangent / aspect;
	outMatrix[5] = -inverseHalfFovTangent;
	outMatrix[10] = farZ / (nearZ - farZ);
	outMatrix[11] = -1.0f;
	outMatrix[14] = farZ * nearZ / (nearZ - farZ);
}

/// @brief A unit-ish quad in the XY plane facing +Z, so the fixed directional light
///        sweeps across it as it rotates about Y.
[[nodiscard]] Mesh MakeQuad() noexcept
{
	Mesh mesh;
	mesh.vertices = {
			MeshVertex(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
			MeshVertex(1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
			MeshVertex(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
			MeshVertex(-1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
	};
	mesh.indices = {0, 1, 2, 0, 2, 3};
	return mesh;
}
} // namespace

int main(int argc, char* argv[]) noexcept
{
	std::signal(SIGINT, [](int) { running = false; });
	std::signal(SIGTERM, [](int) { running = false; });

	Engine hengine;
	hengine.Initialize(argc, (const char**) argv);

	auto* app = hengine.GetApplication();
	if (app == nullptr)
	{
		std::cerr << "Error: Failed to create application" << std::endl;
		return 1;
	}

	auto window =
			OS::CreateWindow("VulkanExample - Rotating Quad (Vulkan)", RequestedWindowWidth, RequestedWindowHeight);
	if (!window)
	{
		std::cerr << "Error: Failed to create window" << std::endl;
		return 1;
	}

	window->SetVisible(true);

	// Vulkan-only: construct the concrete renderer directly (no factory, no inheritance).
	VulkanRenderer renderer;
	if (!renderer.Initialize(window.get()))
	{
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

	float view[MatrixElementCount];
	float proj[MatrixElementCount];
	float model[MatrixElementCount];
	SetPerspective(proj, DegreeToRadian(FieldOfViewDegrees),
				   static_cast<float>(extent.width) / static_cast<float>(extent.height), NearZ, FarZ);
	SetTranslation(view, 0.0f, 0.0f, CameraZ);
	renderer.SetView(view);
	renderer.SetProj(proj);

	float angle = 0.0f;

	while (running && !window->IsClosed())
	{
		app->PollEvents();

		static auto lastTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
		lastTime = currentTime;

		angle += deltaTime * RadiansPerSecond;
		if (angle > TwoPi)
			angle -= TwoPi;

		SetRotationY(model, angle);
		renderer.SetModel(model);

		renderer.BeginFrame();
		renderer.Render(deltaTime);
		renderer.EndFrame();

		std::this_thread::sleep_for(FrameSleep);
	}

	renderer.Shutdown();
	window->Close();
	hengine.ShutDown();

	return 0;
}
