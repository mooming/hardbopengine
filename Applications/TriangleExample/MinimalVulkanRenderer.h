#pragma once

#include "OSAL/Window.h"
// Vulkan headers omitted for minimal stub implementation


namespace hbe {

class MinimalVulkanRenderer {
public:
    MinimalVulkanRenderer() noexcept = default;
    ~MinimalVulkanRenderer();

    bool Initialize(OS::IWindow* window) noexcept;
    void Shutdown() noexcept;

    void BeginFrame() noexcept;
    void EndFrame() noexcept;
    void Render(float deltaTime) noexcept;

private:
    // No Vulkan objects in stub implementation
};

} // namespace hbe
