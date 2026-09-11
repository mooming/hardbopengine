// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#ifdef __UNIT_TEST__
#include "RendererTest.h"

#include "RenderCapabilities.h"
#include "Vulkan/VulkanRenderer.h"

namespace hbe
{
using namespace Renderer;

RendererTest::RendererTest() noexcept
	: TestCollection("RendererTest")
{
}

void RendererTest::prepare()
{
	addTest("VulkanRenderer capabilities are unqueried before Initialize", [](auto& log)
	{
		VulkanRenderer renderer;
		const RenderCapabilities caps = renderer.getCapabilities();

		log << "isDeviceQueried=" << caps.isDeviceQueried << " maxTextureDimension2D=" << caps.maxTextureDimension2D;

		// No device exists yet, so the only honest answer is "unknown". An uninitialised
		// renderer used to answer 4096/16/16 as if it had asked the hardware.
		Assert(!caps.isDeviceQueried, "A renderer without a device must not claim queried capabilities");
		Assert(caps.maxTextureDimension2D == 0, "A renderer without a device must not report a texture limit");
		Assert(caps.deviceName[0] == '\0', "A renderer without a device must not report a device name");
	});

	addTest("VulkanRenderer capabilities match an unknown descriptor", [](auto& log)
	{
		VulkanRenderer renderer;
		const RenderCapabilities caps = renderer.getCapabilities();
		const RenderCapabilities unknown;

		log << "sizeof(RenderCapabilities)=" << sizeof(RenderCapabilities);

		// Field-by-field, because the descriptor is a bit-field struct and holds a fixed array,
		// so memcmp would compare padding and could report a difference that is not one.
		Assert(caps.isDeviceQueried == unknown.isDeviceQueried, "Unqueried flag must match");
		Assert(caps.deviceType == unknown.deviceType, "Unqueried device type must match");
		Assert(caps.maxTextureDimension2D == unknown.maxTextureDimension2D, "Unqueried 2D limit must match");
		Assert(caps.maxVertexAttributes == unknown.maxVertexAttributes, "Unqueried vertex limit must match");
		Assert(caps.supportsComputeShader == unknown.supportsComputeShader, "Unqueried compute flag must match");
	});

	addTest("VulkanRenderer Round Trip", [](auto& log)
	{
		VulkanRenderer renderer;
		OS::Window* window = nullptr;

		// Initialize rejects a null window on purpose - the surface is built from the window,
		// and queue family support is queried against that surface. The assertion said the
		// opposite; it survived only because Assert() is compiled out outside __DEBUG__ builds.
		Assert(!renderer.initialize(window), "Initialize must reject a null window");
		renderer.render(0.016f);
		Assert(!renderer.getCapabilities().isDeviceQueried, "A rejected Initialize must leave capabilities unqueried");
		renderer.shutdown();

		log << "null-window round trip is a safe no-op";
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
