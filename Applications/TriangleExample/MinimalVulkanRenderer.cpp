#include "MinimalVulkanRenderer.h"
#include <iostream>

namespace hbe {

MinimalVulkanRenderer::~MinimalVulkanRenderer() {
    Shutdown();
}

bool MinimalVulkanRenderer::Initialize(OS::IWindow* /*window*/) noexcept {
    // Minimal stub: no actual Vulkan initialization.
    std::cout << "MinimalVulkanRenderer initialized (stub)" << std::endl;
    return true;
}

void MinimalVulkanRenderer::Shutdown() noexcept {
    // No resources to clean up in stub implementation.
    std::cout << "MinimalVulkanRenderer shutdown (stub)" << std::endl;
}

void MinimalVulkanRenderer::BeginFrame() noexcept {
    // Stub – would acquire swapchain image.
}

void MinimalVulkanRenderer::EndFrame() noexcept {
    // Stub – would present swapchain image.
}

void MinimalVulkanRenderer::Render(float /*deltaTime*/) noexcept {
    // Stub – draw a static triangle (output message).
    std::cout << "Rendering triangle (stub)" << std::endl;
}

} // namespace hbe
