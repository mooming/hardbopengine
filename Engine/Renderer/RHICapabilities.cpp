// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RHICapabilities.h"

#include "Config/BuildConfig.h"


namespace hbe
{
namespace Renderer
{

bool RHICapabilities::vulkanChecked = false;
bool RHICapabilities::metalChecked = false;
bool RHICapabilities::dx12Checked = false;
bool RHICapabilities::vulkanSupported = false;
bool RHICapabilities::metalSupported = false;
bool RHICapabilities::dx12Supported = false;

bool RHICapabilities::CheckVulkanSupport() noexcept
{
#if VULKAN_SDK
	return true;
#else
	return false;
#endif
}

bool RHICapabilities::CheckMetalSupport() noexcept
{
#if PLATFORM_OSX
	return true;
#else
	return false;
#endif
}

bool RHICapabilities::CheckDX12Support() noexcept
{
#if defined(PLATFORM_WINDOWS)
	return true;
#else
	return false;
#endif
}

bool RHICapabilities::IsVulkanSupported() noexcept
{
	if (!vulkanChecked)
	{
		vulkanSupported = CheckVulkanSupport();
		vulkanChecked = true;
	}

	return vulkanSupported;
}

bool RHICapabilities::IsMetalSupported() noexcept
{
	if (!metalChecked)
	{
		metalSupported = CheckMetalSupport();
		metalChecked = true;
	}

	return metalSupported;
}

bool RHICapabilities::IsDX12Supported() noexcept
{
	if (!dx12Checked)
	{
		dx12Supported = CheckDX12Support();
		dx12Checked = true;
	}

	return dx12Supported;
}

APIType RHICapabilities::GetPreferredAPI() noexcept
{
#if PLATFORM_OSX
	if (IsMetalSupported())
	{
		return APIType::Metal;
	}
#endif

#if VULKAN_SDK
	if (IsVulkanSupported())
	{
		return APIType::Vulkan;
	}
#endif

#if defined(PLATFORM_WINDOWS)
	if (IsDX12Supported())
	{
		return APIType::DX12;
	}
#endif

#if VULKAN_SDK
	return APIType::Vulkan;
#else
	return APIType::Unknown;
#endif
}

RenderCapabilities RHICapabilities::GetCapabilities(APIType api) noexcept
{
	RenderCapabilities caps;
	caps.apiType = api;

	switch (api)
	{
	case APIType::Vulkan:
		caps.supportsComputeShader = true;
		caps.maxTextureSize = 4096;
		caps.maxVertexAttribs = 16;
		break;

	case APIType::Metal:
		caps.supportsComputeShader = true;
		caps.maxTextureSize = 4096;
		caps.maxVertexAttribs = 31;
		break;

	case APIType::DX12:
		caps.supportsComputeShader = true;
		caps.supportsTessellation = true;
		caps.maxTextureSize = 16384;
		caps.maxVertexAttribs = 32;
		break;

	default:
		break;
	}

	return caps;
}

} // namespace Renderer
} // namespace hbe

#ifdef __UNIT_TEST__

namespace hbe
{

void RHICapabilitiesTest::Prepare()
{
	AddTest("IsMetalSupported", [](auto& ls)
	{
		auto supported = Renderer::RHICapabilities::IsMetalSupported();
		ls << (supported ? "true" : "false");
		Assert(supported, "Metal should be supported on macOS");
	});

	AddTest("GetPreferredAPI", [](auto& ls)
	{
		auto api = Renderer::RHICapabilities::GetPreferredAPI();
		ls << "Preferred: " << static_cast<int>(api);
		Assert(api != Renderer::APIType::Unknown, "Preferred API should not be Unknown");
	});

	AddTest("GetCapabilities", [](auto& ls)
	{
		auto caps = Renderer::RHICapabilities::GetCapabilities(Renderer::APIType::Metal);
		ls << "API: " << static_cast<int>(caps.apiType);
		Assert(caps.apiType == Renderer::APIType::Metal, "API type should match Metal");
	});
}

} // namespace hbe

#endif
