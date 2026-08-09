// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

#include <cstring>
#include <iostream>
#include <string>

#include "Core/CommonMacros.h"
#include "Window.h"
#include "Framebuffer.h"


// ─────────────────────────────────────────────────────────────────────────────
// Objective-C++ Bridge: FramebufferView
// ─────────────────────────────────────────────────────────────────────────────

// A custom NSView subclass that draws framebuffer pixels to the screen.
// It wraps the raw RGBA pixel buffer in a CGImage and draws it in
// drawRect:, which the system calls when the view needs redrawing.
@interface FramebufferView : NSView
{
@public
	const uint32_t* fbPixelData;
	int fbWidth;
	int fbHeight;
	size_t fbPitch;
}

- (instancetype)initWithFrame:(NSRect)frame;
- (void)drawRect:(NSRect)dirtyRect;

@end

@implementation FramebufferView

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self != nil)
    {
        fbPixelData = nullptr;
        fbWidth = 0;
        fbHeight = 0;
        fbPitch = 0;
    }
    return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;

    if (fbPixelData == nullptr || fbWidth <= 0 || fbHeight <= 0)
    {
        return;
    }

    const size_t bytesPerRow = fbPitch;
    const size_t dataLength = bytesPerRow * (size_t)fbHeight;

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (colorSpace == nullptr)
    {
        return;
    }

    // Create a data provider that references our framebuffer memory directly
    // (no copy, no release callback — we own the memory)
    CGDataProviderRef dataProvider = CGDataProviderCreateWithData(
        nullptr,
        fbPixelData,
        dataLength,
        nullptr
    );

    if (dataProvider == nullptr)
    {
        CGColorSpaceRelease(colorSpace);
        return;
    }

    // RGBA on little-endian macOS:
    // kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little
    // matches uint32_t in ARGB layout on little-endian CPUs (0xAARRGGBB)
    CGImageRef image = CGImageCreate(
        (size_t)fbWidth,
        (size_t)fbHeight,
        8,
        32,
        (size_t)bytesPerRow,
        colorSpace,
        (CGBitmapInfo)(kCGImageAlphaPremultipliedFirst) | (CGBitmapInfo)(kCGBitmapByteOrder32Little),
        dataProvider,
        nullptr,
        false,
        kCGRenderingIntentDefault
    );

    CGDataProviderRelease(dataProvider);
    CGColorSpaceRelease(colorSpace);

    if (image == nullptr)
    {
        return;
    }

    // Draw the image into the view's bounds.
    // CGImage is bottom-left origin; our framebuffer is top-left origin.
    // We flip the Y-axis to match.
    NSRect bounds = [self bounds];
    CGContextRef context = [[NSGraphicsContext currentContext] CGContext];

    if (context != nullptr)
    {
        CGContextSaveGState(context);
        CGContextTranslateCTM(context, 0.0, bounds.size.height);
        CGContextScaleCTM(context, 1.0, -1.0);
        CGContextDrawImage(context, bounds, image);
        CGContextRestoreGState(context);
    }

    CGImageRelease(image);
}

@end


// ─────────────────────────────────────────────────────────────────────────────
// Window C++ Implementation
// ─────────────────────────────────────────────────────────────────────────────

namespace hbe
{

Window::Window()
	: nsWindowHandle(nullptr)
	, nsViewHandle(nullptr)
	, width(0)
	, height(0)
	, visibleFlag(false)
	, closedFlag(false)
	, fullScreenFlag(false)
{
	std::memset(lastFrameKeyStates, 0, sizeof(lastFrameKeyStates));
	std::memset(currentKeyStates, 0, sizeof(currentKeyStates));
}

Window::~Window()
{
	Close();
}

bool Window::CreateWindow(const std::string& title, int inWidth, int inHeight)
{
	width = inWidth;
	height = inHeight;

	// Calculate initial window position.
	// macOS screen coordinates: (0,0) is bottom-left.
	// We place the window at (200, 500) so it appears near the top-left
	// of the primary display.
	NSRect frame = NSMakeRect(200, 500, inWidth, inHeight);

	NSUInteger styleMask = NSWindowStyleMaskTitled
	                     | NSWindowStyleMaskClosable
	                     | NSWindowStyleMaskMiniaturizable
	                     | NSWindowStyleMaskResizable;

	NSWindow* nsWindow = [[NSWindow alloc] initWithContentRect:frame
	                                                 styleMask:styleMask
	                                                   backing:NSBackingStoreBuffered
	                                                     defer:NO];

	if (nsWindow == nil)
	{
		std::cerr << "Error: Window::CreateWindow - failed to create NSWindow" << std::endl;
		return false;
	}

	[nsWindow setReleasedWhenClosed:NO];
	[nsWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
	[nsWindow setAcceptsMouseMovedEvents:YES];

	// Enable full-screen support via collection behavior
	[nsWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

	// Create the custom view that draws our framebuffer
	FramebufferView* view = [[FramebufferView alloc] initWithFrame:frame];
	[view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
	[nsWindow setContentView:view];

	nsWindowHandle = static_cast<void*>(nsWindow);
	nsViewHandle = static_cast<void*>(view);

	[nsWindow makeKeyAndOrderFront:nil];
	[nsWindow center];

	visibleFlag = true;
	closedFlag = false;
	fullScreenFlag = false;

	return true;
}

void Window::SetTitle(const std::string& title) noexcept
{
	returnIf(nsWindowHandle == nullptr);

	NSWindow* nsWindow = static_cast<NSWindow*>(nsWindowHandle);
	[nsWindow setTitle:[NSString stringWithUTF8String:title.c_str()]];
}

void Window::SetSize(int inWidth, int inHeight) noexcept
{
	returnIf(nsWindowHandle == nullptr);

	width = inWidth;
	height = inHeight;

	NSWindow* nsWindow = static_cast<NSWindow*>(nsWindowHandle);
	NSRect currentFrame = [nsWindow frame];
	NSRect newFrame = NSMakeRect(NSMinX(currentFrame), NSMinY(currentFrame), inWidth, inHeight);
	[nsWindow setFrame:newFrame display:YES animate:NO];
}

void Window::ToggleFullScreen() noexcept
{
	returnIf(nsWindowHandle == nullptr);

	NSWindow* nsWindow = static_cast<NSWindow*>(nsWindowHandle);
	[nsWindow toggleFullScreen:nil];

	fullScreenFlag = !fullScreenFlag;
}

void Window::PollEvents() noexcept
{
	returnIf(nsWindowHandle == nullptr);

	NSWindow* nsWindow = static_cast<NSWindow*>(nsWindowHandle);

	// Check if the window was closed by the user (traffic light close button)
	if (![nsWindow isVisible])
	{
		closedFlag = true;
		return;
	}

	// Update width/height (may have changed in fullscreen or by resize)
	NSRect frame = [nsWindow frame];
	width = static_cast<int>(frame.size.width);
	height = static_cast<int>(frame.size.height);

	// Swap key state arrays for next-frame edge detection
	std::memcpy(lastFrameKeyStates, currentKeyStates, sizeof(lastFrameKeyStates));

	// Poll ALL events from the application's event queue (keyboard + mouse)
	NSApplication* app = [NSApplication sharedApplication];
	bool mouseUpThisFrame = false;

	while (NSEvent* event = [app nextEventMatchingMask:NSEventMaskAny
	                                          untilDate:[NSDate distantPast]
	                                             inMode:NSDefaultRunLoopMode
	                                          dequeue:YES])
	{
		NSEventType eventType = [event type];

		if (eventType == NSEventTypeKeyDown)
		{
			unsigned short keyCode = [event keyCode];
			if (keyCode < 256)
			{
				currentKeyStates[keyCode] = true;
			}
		}
		else if (eventType == NSEventTypeKeyUp)
		{
			unsigned short keyCode = [event keyCode];
			if (keyCode < 256)
			{
				currentKeyStates[keyCode] = false;
			}
		}
		else if (eventType == NSEventTypeLeftMouseDown)
		{
			NSPoint nsPoint = [event locationInWindow];
			// Flip Y for framebuffer coords (macOS uses bottom-left origin)
			mouseState.x = static_cast<int>(nsPoint.x);
			mouseState.y = height - static_cast<int>(nsPoint.y);
			mouseState.leftButtonPressed = true;
			mouseState.leftButtonReleased = false;
		}
		else if (eventType == NSEventTypeLeftMouseUp)
		{
			NSPoint nsPoint = [event locationInWindow];
			mouseState.x = static_cast<int>(nsPoint.x);
			mouseState.y = height - static_cast<int>(nsPoint.y);
			mouseState.leftButtonPressed = false;
			mouseState.leftButtonReleased = true;
			mouseUpThisFrame = true;
		}
		else if (eventType == NSEventTypeLeftMouseDragged)
		{
			NSPoint nsPoint = [event locationInWindow];
			mouseState.x = static_cast<int>(nsPoint.x);
			mouseState.y = height - static_cast<int>(nsPoint.y);
		}
		else if (eventType == NSEventTypeMouseMoved)
		{
			NSPoint nsPoint = [event locationInWindow];
			mouseState.x = static_cast<int>(nsPoint.x);
			mouseState.y = height - static_cast<int>(nsPoint.y);
		}
		else if (eventType == NSEventTypeScrollWheel)
		{
			mouseState.wheelDeltaY = static_cast<float>([event deltaY]);
		}
		else
		{
			// Forward other events to the application
			[app sendEvent:event];
		}
	}

	// Reset one-shot release flag after processing (edge-triggered)
	if (!mouseUpThisFrame)
	{
		mouseState.leftButtonReleased = false;
	}
}

void Window::Present(const Framebuffer& framebuffer) noexcept
{
	if (nsViewHandle == nullptr || nsWindowHandle == nullptr)
	{
		return;
	}

	// Update the FramebufferView with the latest framebuffer data
	FramebufferView* view = static_cast<FramebufferView*>(nsViewHandle);
	view->fbPixelData = framebuffer.GetData();
	view->fbWidth = framebuffer.GetWidth();
	view->fbHeight = framebuffer.GetHeight();
	view->fbPitch = framebuffer.GetPitch();

	// Trigger redraw of the view
	[view setNeedsDisplay:YES];

	NSWindow* nsWindow = static_cast<NSWindow*>(nsWindowHandle);
	[nsWindow displayIfNeeded];
}

void Window::Close() noexcept
{
	if (nsWindowHandle != nullptr)
	{
		NSWindow* nsWindow = static_cast<NSWindow*>(nsWindowHandle);
		[nsWindow orderOut:nil];
		[nsWindow close];
		[nsWindow release];
		nsWindowHandle = nullptr;
	}

	if (nsViewHandle != nullptr)
	{
		nsViewHandle = nullptr;
	}

	closedFlag = true;
	visibleFlag = false;
}

bool Window::WasKeyPressed(int keyCode) const noexcept
{
	if (keyCode < 0 || keyCode >= 256)
	{
		return false;
	}

	// Pressed this frame but not last frame = edge trigger
	return currentKeyStates[keyCode] && !lastFrameKeyStates[keyCode];
}

const MouseState& Window::GetMouseState() const noexcept
{
	return mouseState;
}

int Window::GetWidth() const noexcept
{
	return width;
}

int Window::GetHeight() const noexcept
{
	return height;
}

bool Window::IsVisible() const noexcept
{
	return visibleFlag;
}

bool Window::IsClosed() const noexcept
{
	return closedFlag;
}

bool Window::IsFullScreen() const noexcept
{
	return fullScreenFlag;
}

void* Window::GetNativeHandle() const noexcept
{
	return nsWindowHandle;
}

} // namespace hbe
