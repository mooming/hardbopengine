// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

#include "Config/BuildConfig.h"
#include "OSAL/Window.h"
#include "RenderCapabilities.h"

namespace hbe
{
namespace Renderer
{
/// API reference: docs/Renderer/MeshVertex/index.html
struct MeshVertex
{
	float position[3];
	float normal[3];

	MeshVertex() = default;

	MeshVertex(float x, float y, float z, float nx, float ny, float nz)
		: position{x, y, z}
		, normal{nx, ny, nz}
	{
	}
};

/// API reference: docs/Renderer/Mesh/index.html
struct Mesh
{
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
};

/// API reference: docs/Renderer/PushConstants/index.html
struct PushConstants
{
	float model[16];
	float viewProj[16];
};

inline void IdentityMatrix(float m[16]) noexcept
{
	for (int i = 0; i < 16; ++i)
		m[i] = 0.0f;
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/// API reference: docs/Renderer/VulkanRenderer/index.html
class VulkanRenderer
{
private:
	static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

	OS::Window* window;
	bool initialized;
	RenderCapabilities capabilities;

	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	uint32_t queueFamilyIndex;
	VkDevice device;
	VkQueue graphicsQueue;
	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surfaceFormat;
	VkPresentModeKHR presentMode;
	VkExtent2D extent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkFormat depthFormat;
	VkImage depthImage;
	VkDeviceMemory depthMemory;
	VkImageView depthImageView;
	VkRenderPass renderPass;
	std::vector<VkFramebuffer> framebuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkShaderModule vertModule;
	VkShaderModule fragModule;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexMemory;
	size_t vertexCount;
	size_t indexCount;
	std::vector<VkSemaphore> semaphoresImageAvailable;
	std::vector<VkSemaphore> semaphoresRenderFinished;
	std::vector<VkFence> fences;
	size_t currentFrame;
	uint32_t imageIndex;
	bool frameActive;
	bool firstFramePresented;

	float modelMat[16];
	float viewMat[16];
	float projMat[16];

public:
	VulkanRenderer() noexcept;
	~VulkanRenderer();

	VulkanRenderer(const VulkanRenderer&) = delete;
	VulkanRenderer& operator=(const VulkanRenderer&) = delete;

	[[nodiscard]] bool Initialize(OS::Window* window) noexcept;
	void Shutdown() noexcept;

	void BeginFrame() noexcept;
	void Render(float deltaTime) noexcept;
	void EndFrame() noexcept;

	[[nodiscard]] RenderCapabilities GetCapabilities() const noexcept;

	void SetMesh(const Mesh& mesh) noexcept;

	void SetModel(const float* m) noexcept;
	void SetView(const float* m) noexcept;
	void SetProj(const float* m) noexcept;

	[[nodiscard]] VkExtent2D GetExtent() const noexcept;

private:
	bool CreateInstance() noexcept;
	bool CreateSurface() noexcept;
	bool PickDevice() noexcept;
	bool CreateDevice() noexcept;

	void QueryCapabilities() noexcept;

	bool CreateSwapchain() noexcept;
	bool CreateRenderPass() noexcept;
	bool CreateDepthResource() noexcept;
	bool CreateFramebuffers() noexcept;
	bool CreateCommandBuffers() noexcept;
	bool CreateVertexBuffers(const Mesh& mesh) noexcept;
	void ReleaseVertexBuffers() noexcept;
	bool CreatePipeline() noexcept;
	bool CreateSyncObjects() noexcept;

	bool CreateMetalSurface() noexcept;

	bool CreateBuffer(VkBuffer& buffer, VkDeviceMemory& memory, size_t size, VkBufferUsageFlags usage) noexcept;
	void RecordFrame() noexcept;
	void Destroy() noexcept;
};
} // namespace Renderer
} // namespace hbe
