// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

// VulkanRenderer.cpp - Cross-platform Vulkan implementation

#include "Config/BuildConfig.h"
#include "VulkanRenderer.h"

#include <cstring>
#include <cmath>

#if defined(PLATFORM_WINDOWS) && defined(VULKAN_SDK)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#endif

#if defined(PLATFORM_LINUX) && defined(VULKAN_SDK)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#endif

namespace hbe
{
namespace Renderer
{

VulkanRenderer::VulkanRenderer()
{
    m_APIType = APIType::Vulkan;
    m_Capabilities.apiType = APIType::Vulkan;
    m_Capabilities.supportsComputeShader = true;
    m_Capabilities.maxTextureSize = 4096;
    m_Capabilities.maxVertexAttribs = 16;
}

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

bool VulkanRenderer::Initialize(OS::IWindow* window)
{
    if (m_Initialized)
    {
        return true;
    }

    m_Window = window;
    m_Initialized = true;
    return true;
}

void VulkanRenderer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    m_Initialized = false;
}

void VulkanRenderer::BeginFrame()
{
}

void VulkanRenderer::EndFrame()
{
}

void VulkanRenderer::Render(float deltaTime)
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

    m_Vertices[0] = {x1, y1, 1.0f, 0.0f, 0.0f, 1.0f};
    m_Vertices[1] = {x2, y2, 0.0f, 1.0f, 0.0f, 1.0f};
    m_Vertices[2] = {x3, y3, 0.0f, 0.0f, 1.0f, 1.0f};
    m_Vertices[3] = {x1, y1, 1.0f, 0.0f, 0.0f, 1.0f};
    m_Vertices[4] = {x3, y3, 0.0f, 0.0f, 1.0f, 1.0f};
    m_Vertices[5] = {x4, y4, 1.0f, 1.0f, 1.0f, 1.0f};
}

APIType VulkanRenderer::GetAPIType() const
{
    return m_APIType;
}

RenderCapabilities VulkanRenderer::GetCapabilities() const
{
    return m_Capabilities;
}

void VulkanRenderer::InitPlatformLayers()
{
}

void VulkanRenderer::SetMetalLayer(void* layer)
{
#if defined(PLATFORM_OSX)
    m_MetalLayer = layer;
#endif
}

} // namespace Renderer
} // namespace hbe