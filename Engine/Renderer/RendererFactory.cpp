// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RendererFactory.h"

#include "IRenderer.h"
#include "Vulkan/VulkanRenderer.h"
#include "Metal/MetalRenderer.h"
#include "DX12/DX12Renderer.h"

namespace hbe
{
namespace Renderer
{

std::unique_ptr<IRenderer> RendererFactory::Create(APIType preferredAPI)
{
	if (preferredAPI == APIType::Unknown)
	{
		preferredAPI = RHICapabilities::GetPreferredAPI();
	}

#if PLATFORM_OSX
	if (preferredAPI == APIType::Metal || preferredAPI == APIType::Unknown)
	{
		IRenderer* renderer = new MetalRenderer();
		return std::unique_ptr<IRenderer>(renderer);
	}
#endif

#if VULKAN_SDK
	if (preferredAPI == APIType::Vulkan || preferredAPI == APIType::Unknown)
	{
		IRenderer* renderer = new VulkanRenderer();
		return std::unique_ptr<IRenderer>(renderer);
	}
#endif

#if defined(PLATFORM_WINDOWS)
	if (preferredAPI == APIType::DX12)
	{
		IRenderer* renderer = new DX12Renderer();
		return std::unique_ptr<IRenderer>(renderer);
	}
#endif

#if VULKAN_SDK
	IRenderer* renderer = new VulkanRenderer();
	return std::unique_ptr<IRenderer>(renderer);
#else
#if PLATFORM_OSX
	IRenderer* renderer = new MetalRenderer();
	return std::unique_ptr<IRenderer>(renderer);
#else
	return std::unique_ptr<IRenderer>(nullptr);
#endif
#endif
}

std::unique_ptr<IRenderer> RendererFactory::CreateWithFallback()
{
	return Create(RHICapabilities::GetPreferredAPI());
}

} // namespace Renderer
} // namespace hbe