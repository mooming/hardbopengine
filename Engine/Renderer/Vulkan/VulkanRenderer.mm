// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

// VulkanRenderer.mm - macOS platform-specific implementation

#include "Config/BuildConfig.h"
#include "VulkanRenderer.h"

#if __has_include("vulkan/vulkan_h")
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#endif

#if defined(PLATFORM_OSX)
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#endif

#include <iostream>

namespace hbe
{
namespace Renderer
{

void VulkanRenderer::InitPlatformLayers()
{
#if defined(PLATFORM_OSX) && __has_include("vulkan/vulkan_h")
    if (m_Window != nullptr && m_MetalLayer == nullptr)
    {
        NSWindow* nsWindow = (NSWindow*)m_Window->GetNativeHandle();
        if (nsWindow != nil)
        {
            NSView* contentView = nsWindow.contentView;
            if (contentView != nil && contentView.layer != nil)
            {
                m_MetalLayer = (__bridge void*)contentView.layer;
            }
        }
    }
#endif
}

void VulkanRenderer::SetMetalLayer(void* layer)
{
#if defined(PLATFORM_OSX)
    m_MetalLayer = layer;
#endif
}

} // namespace Renderer
} // namespace hbe