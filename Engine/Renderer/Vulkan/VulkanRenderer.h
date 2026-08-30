// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#if defined(VULKAN_SDK) && __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#if defined(PLATFORM_OSX)
#include <vulkan/vulkan_metal.h>
#endif
#endif

#include "Config/BuildConfig.h"
#include "OSAL/Window.h"
#include "RendererCommon.h"

namespace hbe
{
namespace Renderer
{

struct QuadVertex {
    float x, y;
    float r, g, b, a;
};

/// @brief Concrete Vulkan renderer.
/// @details No inheritance and no factory: the renderer is constructed directly.
///          Platform-specific plumbing is split like the OSAL Window module -
///          the core lives in VulkanRenderer.cpp (Linux/Windows) and the macOS
///          metal-layer bridge lives in VulkanRenderer.mm.
class VulkanRenderer
{
public:
    VulkanRenderer() noexcept;
    ~VulkanRenderer();

    [[nodiscard]] bool Initialize(OS::Window* window) noexcept;
    void Shutdown() noexcept;

    void BeginFrame() noexcept;
    void EndFrame() noexcept;

    void Render(float deltaTime) noexcept;

    [[nodiscard]] APIType GetAPIType() const noexcept;
    [[nodiscard]] RenderCapabilities GetCapabilities() const noexcept;

    void SetMetalLayer(void* layer) noexcept;

private:
    void InitPlatformLayers() noexcept;

    OS::Window* window;
    APIType apiType;
    RenderCapabilities capabilities;

#if defined(PLATFORM_OSX)
    void* metalLayer;
#endif

    float rotationAngle;
    bool initialized;
    QuadVertex vertices[6];
};

} // namespace Renderer
} // namespace hbe
