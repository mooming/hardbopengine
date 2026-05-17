// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

// VulkanRenderer.mm - macOS platform-specific implementation

#include "VulkanRenderer.h"

#if __has_include("vulkan/vulkan_h")
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#endif

#if defined(PLATFORM_OSX)
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#endif


namespace hbe
{
namespace Renderer
{

void VulkanRenderer::InitPlatformLayers() noexcept
{
#if defined(PLATFORM_OSX) && __has_include("vulkan/vulkan_h")
    if (window != nullptr && metalLayer == nullptr)
    {
        NSWindow* nsWindow = (NSWindow*)window->GetNativeHandle();
        if (nsWindow != nil)
        {
            NSView* contentView = nsWindow.contentView;
            if (contentView != nil && contentView.layer != nil)
            {
                metalLayer = (__bridge void*)contentView.layer;
            }
        }
    }
#endif
}

void VulkanRenderer::SetMetalLayer(void* layer) noexcept
{
#if defined(PLATFORM_OSX)
    metalLayer = layer;
#endif
}

} // namespace Renderer
} // namespace hbe
