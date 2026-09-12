// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

// VulkanRenderer.mm - macOS platform surface (Vulkan <-> Metal interop).

#include "Vulkan/VulkanRenderer.h"

#if defined(PLATFORM_OSX)

#include "Core/CommonMacros.h"
#include "Log/Logger.h"

#include <vulkan/vulkan.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace hbe
{
namespace Renderer
{

namespace
{
void LogSurfaceFailure(const char* reason) noexcept
{
	static const auto log = Logger::Get("VulkanRenderer", ELogLevel::Error);
	log.OutError(reason);
}
} // namespace

bool VulkanRenderer::CreateMetalSurface() noexcept
{
	if (window == nullptr || instance == VK_NULL_HANDLE) return false;

	NSWindow* nsWindow = static_cast<NSWindow*>(reinterpret_cast<void*>(window->GetNativeHandle()));
	if (nsWindow == nil)
	{
		LogSurfaceFailure("window native handle is not an NSWindow");
		return false;
	}

	// MRC: MTLCreateSystemDefaultDevice follows the create rule, so it is released below
	// once the layer has retained it.
	id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
	if (metalDevice == nil)
	{
		LogSurfaceFailure("MTLCreateSystemDefaultDevice returned no Metal device");
		return false;
	}

	NSView* contentView = [[NSView alloc] initWithFrame:[nsWindow contentRectForFrameRect:[nsWindow frame]]];
	if (contentView == nil)
	{
		LogSurfaceFailure("failed to allocate the presentation content view");
		[metalDevice release];
		return false;
	}

	CAMetalLayer* layer = [CAMetalLayer layer];
	if (layer == nil)
	{
		LogSurfaceFailure("failed to create the CAMetalLayer");
		[metalDevice release];
		[contentView release];
		return false;
	}

	layer.device = metalDevice;
	[metalDevice release];
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = YES;

	// contentsScale 1.0 keeps drawable pixels equal to points. AppKit sizes the layer to
	// the view bounds, so CAMetalLayer.drawableSize - and therefore the swapchain extent -
	// follows the window's content rect; CreateSwapchain reads the authoritative size from
	// VkSurfaceCapabilitiesKHR::currentExtent instead of assuming the requested window size.
	layer.contentsScale = 1.0;

	// AppKit ignores +layerClass here (it installs its own NSViewBackingLayer), so the
	// CAMetalLayer is created explicitly and adopted by the layer-backed view.
	contentView.wantsLayer = YES;
	contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	contentView.layer = layer;

	[nsWindow setContentView:contentView];
	[contentView release]; // the window retains its content view

	VkMetalSurfaceCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
	info.pLayer = layer;

	const VkResult result = vkCreateMetalSurfaceEXT(instance, &info, nullptr, &surface);
	if (result != VK_SUCCESS)
	{
		static const auto log = Logger::Get("VulkanRenderer", ELogLevel::Error);
		log.OutError([&](auto& ls)
		{
			ls << "vkCreateMetalSurfaceEXT failed (VkResult=" << static_cast<int>(result) << ")";
		});
		return false;
	}

	return true;
}

} // namespace Renderer
} // namespace hbe

#endif // PLATFORM_OSX
