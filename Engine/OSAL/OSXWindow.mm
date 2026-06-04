// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSXWindow.h"

#ifdef PLATFORM_OSX
#import <Cocoa/Cocoa.h>
#include <cstdio>

namespace OS
{

OSXWindow::OSXWindow()
	: nsWindow(nullptr)
	, width(0)
	, height(0)
	, visibleFlag(false)
	, closedFlag(false)
{
}

OSXWindow::~OSXWindow()
{
	Close();
}

bool OSXWindow::CreateWindow(const hbe::HString& title, int width, int height)
{
	if (nsWindow != nullptr)
		return false;

	NSRect frame = NSMakeRect(100, 100, width, height);
	NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;

	NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
												  styleMask:styleMask
													backing:NSBackingStoreBuffered
													  defer:NO];

	if (window == nullptr)
		return false;

	[window setReleasedWhenClosed:NO];
	[window setTitle:[NSString stringWithUTF8String:title.c_str()]];
	[window setFrame:frame display:YES animate:NO];
	[window makeKeyAndOrderFront:nil];
	[window center];

	OSXWindow::width = width;
	OSXWindow::height = height;
	visibleFlag = true;
	nsWindow = window;

	return true;
}

void OSXWindow::SetTitle(const hbe::HString& title)
{
	if (nsWindow == nullptr)
		return;

	auto window = static_cast<NSWindow*>(nsWindow);
	[window setTitle:[NSString stringWithUTF8String:title.c_str()]];
}

void OSXWindow::SetSize(int width, int height)
{
	if (nsWindow == nullptr)
		return;

	auto window = static_cast<NSWindow*>(nsWindow);
	NSRect frame = [window frame];
	frame.size.width = width;
	frame.size.height = height;
	[window setFrame:frame display:YES animate:NO];
	OSXWindow::width = width;
	OSXWindow::height = height;
}

int OSXWindow::GetWidth() const
{
	return width;
}

int OSXWindow::GetHeight() const
{
	return height;
}

bool OSXWindow::IsVisible() const
{
	return visibleFlag;
}

void OSXWindow::SetVisible(bool visible)
{
	if (nsWindow == nullptr)
		return;

	auto window = static_cast<NSWindow*>(nsWindow);

	if (visible)
	{
		[window makeKeyAndOrderFront:nil];
	}
	else
	{
		[window orderOut:nil];
	}

	visibleFlag = visible;
}

void OSXWindow::PollEvents()
{
	if (nsWindow != nullptr)
	{
		auto window = static_cast<NSWindow*>(nsWindow);
		if (![window isVisible])
		{
			closedFlag = true;
		}
	}
}

void OSXWindow::Close()
{
	if (nsWindow != nullptr)
	{
		auto window = static_cast<NSWindow*>(nsWindow);
		[window close];
		[window release];
		nsWindow = nullptr;
	}

	closedFlag = true;
}

bool OSXWindow::IsClosed() const
{
	return closedFlag;
}

void OSXWindow::SetPixels(const uint32_t* pixels, int width, int height)
{
	if (nsWindow == nullptr)
		return;

	auto window = static_cast<NSWindow*>(nsWindow);
	NSView *contentView = [window contentView];

	// Create a bitmap context
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	if (!colorSpace)
		return;

	CGContextRef context = CGBitmapContextCreate(
		const_cast<uint32_t*>(pixels),
		width,
		height,
		8,
		width * 4,
		colorSpace,
		kCGImageAlphaNoneSkipFirst
	);

	if (context)
	{
		CGImageRef image = CGBitmapContextCreateImage(context);
		if (image)
		{
			// Create an NSImage from the CGImage
			NSImage *nsImage = [[NSImage alloc] initWithCGImage:image size:NSMakeSize(width, height)];
			if (nsImage)
			{
				// Create a bitmap representation and set it as the window content
				NSBitmapImageRep *bitmapRep = [[NSBitmapImageRep alloc]
					initWithBitmapDataPlanes:nullptr
					pixelsWide:width
					pixelsHigh:height
					samplesPerPixel:4
					hasAlpha:YES
					planar:NO
					colorSpaceName:NSDeviceRGBColorSpace
					bitmapFormat:NSBitmapFormat32LittleEndian
					bytesPerRow:width * 4
					bitsPerPixel:32];

				if (bitmapRep)
				{
					[bitmapRep getBitmapDataPlanes:nullptr];
					uint32_t *bitmapData = static_cast<uint32_t*>([bitmapRep bitmapData]);
					if (bitmapData)
					{
						// Copy pixel data (ARGB to BGRA for macOS)
						const uint32_t* src = pixels;
						for (int i = 0; i < width * height; ++i)
						{
							// Swap A and B for macOS (ABGR -> BGRA)
							uint32_t pixel = src[i];
							bitmapData[i] = ((pixel & 0xFF00FF00) | ((pixel << 16) & 0xFF0000) | ((pixel >> 16) & 0xFF));
						}
					}

					[bitmapRep unlockFocus];

					// Create an NSImage from the bitmap representation
					NSImage *windowImage = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
					[windowImage addRepresentation:bitmapRep];

					// Set the image as the window content
					[window setContentImage:windowImage];

					[bitmapRep release];
				}
				[nsImage release];
			}
			CGImageRelease(image);
		}
		CGContextRelease(context);
	}

	CGColorSpaceRelease(colorSpace);
}

intptr_t OSXWindow::GetNativeHandle() const
{
	return reinterpret_cast<intptr_t>(nsWindow);
}

} // namespace OS

#endif // PLATFORM_OSX
