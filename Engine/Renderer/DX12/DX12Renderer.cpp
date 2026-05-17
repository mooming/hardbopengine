// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "DX12Renderer.h"

#include <cmath>
#include <cstring>

#include "Config/BuildConfig.h"


namespace hbe
{
namespace Renderer
{

DX12Renderer::DX12Renderer() noexcept
	: apiType(APIType::DX12)
{
	capabilities.apiType = APIType::DX12;
	capabilities.supportsComputeShader = true;
	capabilities.supportsTessellation = true;
	capabilities.maxTextureSize = 16384;
	capabilities.maxVertexAttribs = 32;
}

DX12Renderer::~DX12Renderer()
{
	Shutdown();
}

bool DX12Renderer::Initialize(OS::IWindow* window) noexcept
{
	HBE_UNUSED(window);

	if (initialized) return true;

	initialized = true;

	return true;
}

void DX12Renderer::Shutdown() noexcept
{
	if (!initialized) return;

	device = 0;
	commandQueue = 0;
	commandList = 0;
	vertexBuffer = 0;

	initialized = false;
}

void DX12Renderer::BeginFrame() noexcept
{
}

void DX12Renderer::EndFrame() noexcept
{
}

void DX12Renderer::Render(float deltaTime) noexcept
{
	rotationAngle += deltaTime * 90.0f;
	if (rotationAngle > 360.0f)
	{
		rotationAngle -= 360.0f;
	}

	float rad = rotationAngle * 3.14159265359f / 180.0f;
	float c = cosf(rad);
	float s = sinf(rad);

	float x1 = -0.5f * c - (-0.5f) * s;
	float y1 = -0.5f * s + (-0.5f) * c;
	float x2 = 0.5f * c - (-0.5f) * s;
	float y2 = 0.5f * s + (-0.5f) * c;
	float x3 = 0.5f * c - 0.5f * s;
	float y3 = 0.5f * s + 0.5f * c;
	float x4 = -0.5f * c - 0.5f * s;
	float y4 = -0.5f * s + 0.5f * c;

	DX12Vertex vertices[6] = {
		{x1, y1,  1.0f, 0.0f, 0.0f, 1.0f},
		{x2, y2,  0.0f, 1.0f, 0.0f, 1.0f},
		{x3, y3,  0.0f, 0.0f, 1.0f, 1.0f},
		{x1, y1,  1.0f, 0.0f, 0.0f, 1.0f},
		{x3, y3,  0.0f, 0.0f, 1.0f, 1.0f},
		{x4, y4,  1.0f, 1.0f, 1.0f, 1.0f}
	};

	(void)vertices;
	(void)vertexBuffer;
}

APIType DX12Renderer::GetAPIType() const noexcept
{
	return apiType;
}

RenderCapabilities DX12Renderer::GetCapabilities() const noexcept
{
	return capabilities;
}

} // namespace Renderer
} // namespace hbe
