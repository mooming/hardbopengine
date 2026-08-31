// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include "Config/BuildConfig.h"
#include "OSAL/Window.h"
#include "RendererCommon.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace hbe
{
namespace Renderer
{

/// @brief Single vertex: position (vec3) + normal (vec3), 24 bytes, tightly packed.
struct MeshVertex
{
	float position[3];
	float normal[3];

	MeshVertex() = default;

	MeshVertex(float x, float y, float z, float nx, float ny, float nz)
		: position{x, y, z}, normal{nx, ny, nz}
	{}
};

/// @brief A triangle mesh to upload to the GPU.
struct Mesh
{
	std::vector<MeshVertex> vertices;
	std::vector<uint32_t> indices;
};

/// @brief Push constants consumed per frame by Shaders/MC.vert.
/// @details Must mirror the GLSL push block exactly: `mat4 view; mat4 proj;`
///          = 128 bytes, which is the portability-guaranteed maxPushConstantsSize.
///          Column-major (GL/Vulkan convention).
struct PushConstants
{
	float view[16];
	float proj[16];
};

inline void IdentityMatrix(float m[16]) noexcept
{
	for (int i = 0; i < 16; ++i) m[i] = 0.0f;
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/// @brief Concrete Vulkan renderer (no inheritance).
/// @details The cross-platform Vulkan pipeline lives in VulkanRenderer.cpp; the
///          macOS surface is created in VulkanRenderer.mm (OSAL-style split).
class VulkanRenderer
{
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

	[[nodiscard]] APIType GetAPIType() const noexcept;
	[[nodiscard]] RenderCapabilities GetCapabilities() const noexcept;

	/// @brief Upload (or replace) the mesh to render.
	void SetMesh(const Mesh& mesh) noexcept;

	/// @brief Set the view / projection matrices (column-major, 16 floats each).
	void SetView(const float* m) noexcept;
	void SetProj(const float* m) noexcept;

private:
	static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

	bool CreateInstance() noexcept;
	bool CreateSurface() noexcept;
	bool PickDevice() noexcept;
	bool CreateDevice() noexcept;
	bool CreateSwapchain() noexcept;
	bool CreateRenderPass() noexcept;
	bool CreateDepthResource() noexcept;
	bool CreateFramebuffers() noexcept;
	bool CreateCommandBuffers() noexcept;
	bool CreateVertexBuffers(const Mesh& mesh) noexcept;
	void ReleaseVertexBuffers() noexcept;
	bool CreatePipeline() noexcept;
	bool CreateSyncObjects() noexcept;

	/// @brief Platform window surface - implemented per platform (see .cpp / .mm).
	bool CreateMetalSurface() noexcept;

	bool CreateBuffer(VkBuffer& buffer, VkDeviceMemory& memory, size_t size, VkBufferUsageFlags usage) noexcept;
	void RecordFrame() noexcept;
	void Destroy() noexcept;

	OS::Window* window;
	bool initialized;
	APIType apiType;
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

	// Scene state (set by the example, pushed to the vertex shader each frame).
	float viewMat[16];
	float projMat[16];
};

} // namespace Renderer
} // namespace hbe
