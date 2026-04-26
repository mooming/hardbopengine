// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Config/BuildConfig.h"
#include "VulkanRenderer.h"

#if VULKAN_SDK
#include <vulkan/vulkan.h>
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

#if VULKAN_SDK
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanExample";

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	VkResult result = vkCreateInstance(&createInfo, nullptr, (VkInstance*)&m_Instance);
	if (result != VK_SUCCESS)
	{
		return false;
	}
#endif

	m_Initialized = true;
	return true;
}

void VulkanRenderer::Shutdown()
{
	if (!m_Initialized)
	{
		return;
	}

#if VULKAN_SDK
	if (m_Instance != 0)
	{
		vkDestroyInstance((VkInstance)m_Instance, nullptr);
		m_Instance = 0;
	}
#endif

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
	m_RotationAngle += deltaTime * 0.5f;
	if (m_RotationAngle > 360.0f)
	{
		m_RotationAngle -= 360.0f;
	}
	(void)deltaTime;
}

APIType VulkanRenderer::GetAPIType() const
{
	return m_APIType;
}

RenderCapabilities VulkanRenderer::GetCapabilities() const
{
	return m_Capabilities;
}

} // namespace Renderer
} // namespace hbe