// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "RHICapabilities.h"

#include <cstring>

#include "Config/BuildConfig.h"
#include "Core/Debug.h"
#include "Vulkan/VulkanCapabilities.h"

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

RenderCapabilities RHICapabilities::GetCapabilities() noexcept
{
	// Real values, read from the driver - see VulkanCapabilities for the translation. The
	// descriptor self-reports as unqueried when no device answers, so callers can tell the
	// difference between "this machine has nothing" and "nobody asked".
	RenderCapabilities capabilities;
	QueryDefaultDeviceCapabilities(capabilities);

	return capabilities;
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

	AddTest("RenderCapabilities starts explicitly unknown", [](auto& ls)
	{
		const Renderer::RenderCapabilities caps;

		ls << "isDeviceQueried=" << caps.isDeviceQueried << " maxTextureDimension2D=" << caps.maxTextureDimension2D;

		// The bug this guards: the descriptor used to ship 4096/16/16 and
		// supportsTessellation=false as defaults, which real hardware contradicted and which
		// no caller could distinguish from a genuine query result.
		Assert(!caps.isDeviceQueried, "A fresh descriptor must not claim to describe a device");
		Assert(caps.maxTextureDimension2D == 0, "Unqueried limit must be zero, not a guessed default");
		Assert(caps.maxVertexAttributes == 0, "Unqueried vertex attribute limit must be zero");
		Assert(caps.maxUniformBufferBindings == 0, "Unqueried uniform buffer limit must be zero");
		Assert(caps.deviceName[0] == '\0', "Unqueried device name must be empty");
		Assert(!caps.supportsTessellation, "Unqueried feature flag must be false");
		Assert(caps.deviceType == Renderer::DeviceType::Unknown, "Unqueried device type must be Unknown");
	});

	AddTest("GetCapabilities reports what the device supports", [](auto& ls)
	{
		const Renderer::RenderCapabilities caps = Renderer::RHICapabilities::GetCapabilities();

		if (!caps.isDeviceQueried)
		{
			// No loader, no ICD, or an incompatible driver - a legitimate environment (headless
			// CI, a machine without MoltenVK), so there is nothing to compare against.
			ls << "no Vulkan device available; query skipped";
			return;
		}

		ls << "device=\"" << caps.deviceName << "\" api=" << static_cast<int>(caps.apiVersionMajor) << "."
		   << static_cast<int>(caps.apiVersionMinor) << " driver=" << caps.driverVersion << " vendor=0x"
		   << caps.vendorId << " tex2D=" << caps.maxTextureDimension2D << " vertexAttribs=" << caps.maxVertexAttributes
		   << " uniformBuffers=" << caps.maxUniformBufferBindings << " colorAttachments=" << caps.maxColorAttachments
		   << " tessellation=" << caps.supportsTessellation << " geometryShader=" << caps.supportsGeometryShader
		   << " compute=" << caps.supportsComputeShader;

		// Identity has to survive the fixed-size copy, and the API version has to be real.
		Assert(caps.deviceName[0] != '\0', "A queried device must report a name");
		Assert(caps.apiVersionMajor >= 1, "A Vulkan device must report API version 1 or newer");
		Assert(caps.deviceType != Renderer::DeviceType::Unknown, "A queried device must classify itself");

		// Limits must be non-zero, and must not be the numbers the old hardcode claimed.
		Assert(caps.maxTextureDimension2D >= 2048, "A Vulkan device supports at least 2048 texels");
		Assert(caps.maxTextureDimension2D != 4096 || caps.maxVertexAttributes != 16,
			   "Values identical to the retired hardcode suggest nothing was actually queried");
		Assert(caps.maxVertexAttributes > 0, "Vertex attribute limit must be queried");
		Assert(caps.maxUniformBufferBindings > 0, "Uniform buffer limit must be queried");
		Assert(caps.maxColorAttachments > 0, "Color attachment limit must be queried");
		Assert(caps.uniformBufferOffsetAlignment > 0, "Uniform buffer alignment must be queried");

		// Vulkan mandates compute in core, so a queried device always reports it.
		Assert(caps.supportsComputeShader, "Vulkan always supports compute shaders");
	});

	AddTest("Repeated probes are stable", [](auto& ls)
	{
		const Renderer::RenderCapabilities first = Renderer::RHICapabilities::GetCapabilities();
		const Renderer::RenderCapabilities second = Renderer::RHICapabilities::GetCapabilities();

		ls << "queried=" << first.isDeviceQueried;
		Assert(first.isDeviceQueried == second.isDeviceQueried, "Two probes must agree on availability");
		Assert(first.maxTextureDimension2D == second.maxTextureDimension2D, "Two probes must agree on limits");
		Assert(std::strcmp(first.deviceName, second.deviceName) == 0, "Two probes must agree on the device name");
	});
}

} // namespace hbe

#endif
