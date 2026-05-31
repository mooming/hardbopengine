#pragma once

#include "OSAL/Window.h"
// Vulkan headers omitted for minimal stub implementation
#include <cstdint>
#include <vector>

namespace hbe
{

class MinimalVulkanRenderer
{
public:
	MinimalVulkanRenderer() noexcept = default;
	~MinimalVulkanRenderer();

	bool Initialize(OS::IWindow* window) noexcept;
	void Shutdown() noexcept;

	void BeginFrame() noexcept;
	void EndFrame() noexcept;
	void Render(float deltaTime) noexcept;

private:
	// Simple software framebuffer for demonstration
	int width = 640;
	int height = 480;
	int frameIndex = 0;

	std::vector<uint32_t> framebuffer;
};

} // namespace hbe
