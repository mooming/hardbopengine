// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "IRenderer.h"

#if defined(PLATFORM_OSX) && defined(VULKAN_SDK)
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#endif

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
    VulkanRenderer();
    ~VulkanRenderer() override;

    bool Initialize(OS::IWindow* window) override;
    void Shutdown() override;

    void BeginFrame() override;
    void EndFrame() override;

    void Render(float deltaTime) override;

    APIType GetAPIType() const override;
    RenderCapabilities GetCapabilities() const override;

    void SetMetalLayer(void* layer);

private:
    void InitPlatformLayers();

    OS::IWindow* m_Window = nullptr;
    APIType m_APIType = APIType::Vulkan;
    RenderCapabilities m_Capabilities;

#if defined(PLATFORM_OSX)
    void* m_MetalLayer = nullptr;
#endif

    float m_RotationAngle = 0.0f;
    bool m_Initialized = false;
    QuadVertex m_Vertices[6];
};

} // namespace Renderer
} // namespace hbe