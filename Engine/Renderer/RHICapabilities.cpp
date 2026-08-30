// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RHICapabilities.h"

#include "Config/BuildConfig.h"

namespace hbe
{
namespace Renderer
{

bool RHICapabilities::IsVulkanSupported() noexcept
{
#if VULKAN_SDK
	return true;
#else
	return false;
#endif
}

APIType RHICapabilities::GetPreferredAPI() noexcept
{
	return APIType::Vulkan;
}

RenderCapabilities RHICapabilities::GetCapabilities() noexcept
{
	RenderCapabilities caps;
	caps.apiType = APIType::Vulkan;
	caps.supportsComputeShader = true;
	caps.maxTextureSize = 4096;
	caps.maxVertexAttribs = 16;

	return caps;
}

} // namespace Renderer
} // namespace hbe

#ifdef __UNIT_TEST__

namespace hbe
{

void RHICapabilitiesTest::Prepare()
{
	AddTest("IsVulkanSupported", [](auto& ls)
	{
		auto supported = Renderer::RHICapabilities::IsVulkanSupported();
		ls << (supported ? "true" : "false");
	});

	AddTest("GetPreferredAPI", [](auto& ls)
	{
		auto api = Renderer::RHICapabilities::GetPreferredAPI();
		ls << "Preferred: " << static_cast<int>(api);
		Assert(api == Renderer::APIType::Vulkan, "Preferred API should be Vulkan");
	});

	AddTest("GetCapabilities", [](auto& ls)
	{
		auto caps = Renderer::RHICapabilities::GetCapabilities();
		ls << "API: " << static_cast<int>(caps.apiType);
		Assert(caps.apiType == Renderer::APIType::Vulkan, "API type should be Vulkan");
	});
}

} // namespace hbe

#endif
