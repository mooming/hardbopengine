// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "DX12Renderer.h"

#include "Config/BuildConfig.h"

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

#if defined(PLATFORM_WINDOWS)
	m_Initialized = true;
#endif

	return m_Initialized;
}

void DX12Renderer::Shutdown()
{
	if (!m_Initialized)
	{
		return;
	}

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
	m_RotationAngle += deltaTime * 1.0f;
	if (m_RotationAngle > 360.0f)
	{
		m_RotationAngle -= 360.0f;
	}
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