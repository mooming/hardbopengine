// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "DX12Renderer.h"

#include "Config/BuildConfig.h"
#include <cstring>
#include <cmath>

namespace hbe
{
namespace Renderer
{

DX12Renderer::DX12Renderer()
{
	m_APIType = APIType::DX12;
	m_Capabilities.apiType = APIType::DX12;
	m_Capabilities.supportsComputeShader = true;
	m_Capabilities.supportsTessellation = true;
	m_Capabilities.maxTextureSize = 16384;
	m_Capabilities.maxVertexAttribs = 32;
}

DX12Renderer::~DX12Renderer()
{
	Shutdown();
}

bool DX12Renderer::Initialize(OS::IWindow* window)
{
	HBE_UNUSED(window);

	if (m_Initialized)
	{
		return true;
	}

	m_Initialized = true;
	return true;
}

void DX12Renderer::Shutdown()
{
	if (!m_Initialized)
	{
		return;
	}

	m_Device = 0;
	m_CommandQueue = 0;
	m_CommandList = 0;
	m_VertexBuffer = 0;

	m_Initialized = false;
}

void DX12Renderer::BeginFrame()
{
}

void DX12Renderer::EndFrame()
{
}

void DX12Renderer::Render(float deltaTime)
{
	m_RotationAngle += deltaTime * 90.0f;
	if (m_RotationAngle > 360.0f)
	{
		m_RotationAngle -= 360.0f;
	}

	float rad = m_RotationAngle * 3.14159265359f / 180.0f;
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
	(void)m_VertexBuffer;
}

APIType DX12Renderer::GetAPIType() const
{
	return m_APIType;
}

RenderCapabilities DX12Renderer::GetCapabilities() const
{
	return m_Capabilities;
}

} // namespace Renderer
} // namespace hbe