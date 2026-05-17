// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#if defined(PLATFORM_OSX) && defined(VULKAN_SDK)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#endif

#include "Config/BuildConfig.h"
#include "IRenderer.h"

namespace hbe
{
namespace Renderer
{

struct QuadVertex {
    float x, y;
    float r, g, b, a;
};

class VulkanRenderer final : public IRenderer
{
public:
    VulkanRenderer() noexcept;
    ~VulkanRenderer() override;

    [[nodiscard]] bool Initialize(OS::IWindow* window) noexcept override;
    void Shutdown() noexcept override;

    void BeginFrame() noexcept override;
    void EndFrame() noexcept override;

    void Render(float deltaTime) noexcept override;

    [[nodiscard]] APIType GetAPIType() const noexcept override;
    [[nodiscard]] RenderCapabilities GetCapabilities() const noexcept override;

    void SetMetalLayer(void* layer) noexcept;

private:
    void InitPlatformLayers() noexcept;

    OS::IWindow* window;
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