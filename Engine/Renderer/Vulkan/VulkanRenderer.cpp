// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "VulkanRenderer.h"

#include <cmath>
#include <cstring>

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

VulkanRenderer::VulkanRenderer() noexcept
    : apiType(APIType::Vulkan)
{
    capabilities.apiType = APIType::Vulkan;
    capabilities.supportsComputeShader = true;
    capabilities.maxTextureSize = 4096;
    capabilities.maxVertexAttribs = 16;
}

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

bool VulkanRenderer::Initialize(OS::IWindow* window) noexcept
{
    if (initialized) return true;

    this->window = window;
    initialized = true;

    return true;
}

void VulkanRenderer::Shutdown() noexcept
{
    if (!initialized) return;

    initialized = false;
}

void VulkanRenderer::BeginFrame() noexcept
{
}

void VulkanRenderer::EndFrame() noexcept
{
}

void VulkanRenderer::Render(float deltaTime) noexcept
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

    vertices[0] = {x1, y1, 1.0f, 0.0f, 0.0f, 1.0f};
    vertices[1] = {x2, y2, 0.0f, 1.0f, 0.0f, 1.0f};
    vertices[2] = {x3, y3, 0.0f, 0.0f, 1.0f, 1.0f};
    vertices[3] = {x1, y1, 1.0f, 0.0f, 0.0f, 1.0f};
    vertices[4] = {x3, y3, 0.0f, 0.0f, 1.0f, 1.0f};
    vertices[5] = {x4, y4, 1.0f, 1.0f, 1.0f, 1.0f};
}

APIType VulkanRenderer::GetAPIType() const noexcept
{
    return apiType;
}

RenderCapabilities VulkanRenderer::GetCapabilities() const noexcept
{
    return capabilities;
}

void VulkanRenderer::InitPlatformLayers() noexcept
{
}

void VulkanRenderer::SetMetalLayer(void* layer) noexcept
{
#if defined(PLATFORM_OSX)
    metalLayer = layer;
#endif
}

} // namespace Renderer
} // namespace hbe
