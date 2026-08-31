// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Window.h"
#include "Core/CommonMacros.h"
#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX
#import <Cocoa/Cocoa.h>
#include <cstdio>

@interface HBWindowDelegate : NSObject <NSWindowDelegate>
{
@public
	bool* closedFlag;
}
@end

@implementation HBWindowDelegate
// Event-driven close detection: the red button lands here, so the owning Window is
// marked closed immediately. Relying on Window::PollEvents to notice an invisible
// window does not work, because applications pump OS::Application::PollEvents instead.
- (BOOL)windowShouldClose:(NSWindow*)sender
{
	if (closedFlag != nullptr)
	{
		*closedFlag = true;
	}

	return YES;
}
@end

namespace OS
{

Window::Window() noexcept
	: width(0)
	, height(0)
	, visibleFlag(false)
	, closedFlag(false)
	, osHandle(nullptr)
	, osDelegate(nullptr)
{
}

Window::~Window()
{
	Close();
}

bool Window::CreateWindow(const hbe::HString& title, int width, int height)
{
	returnValueIf(false, osHandle != nullptr);

	NSRect frame = NSMakeRect(100, 100, width, height);
	NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;

	NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
												  styleMask:styleMask
													backing:NSBackingStoreBuffered
													  defer:NO];

	returnValueIf(false, window == nullptr);

	[window setReleasedWhenClosed:NO];
	[window setTitle:[NSString stringWithUTF8String:title.c_str()]];
	[window setFrame:frame display:YES animate:NO];
	[window makeKeyAndOrderFront:nil];
	[window center];

	// NSWindow's delegate is unretained, so this +1 is ours and is released in Close().
	HBWindowDelegate* windowDelegate = [[HBWindowDelegate alloc] init];
	windowDelegate->closedFlag = &closedFlag; // this Window's member, not a pointer-to-member
	[window setDelegate:windowDelegate];
	osDelegate = windowDelegate;

	Window::width = width;
	Window::height = height;
	visibleFlag = true;
	osHandle = window;

	return true;
}

void Window::SetTitle(const hbe::HString& title)
{
	returnIf(osHandle == nullptr);

	auto window = static_cast<NSWindow*>(osHandle);
	[window setTitle:[NSString stringWithUTF8String:title.c_str()]];
}

void Window::SetSize(int width, int height)
{
	returnIf(osHandle == nullptr);

	auto window = static_cast<NSWindow*>(osHandle);
	NSRect frame = [window frame];
	frame.size.width = width;
	frame.size.height = height;
	[window setFrame:frame display:YES animate:NO];
	Window::width = width;
	Window::height = height;
}

int Window::GetWidth() const
{
	return width;
}

int Window::GetHeight() const
{
	return height;
}

bool Window::IsVisible() const
{
	return visibleFlag;
}

void Window::SetVisible(bool visible)
{
	if (osHandle == nullptr)
	{
		return;
	}

	auto window = static_cast<NSWindow*>(osHandle);

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

void Window::PollEvents()
{
	if (osHandle != nullptr)
	{
		auto window = static_cast<NSWindow*>(osHandle);
		if (![window isVisible])
		{
			closedFlag = true;
		}
	}
}

void Window::Close()
{
	if (osHandle != nullptr)
	{
		auto window = static_cast<NSWindow*>(osHandle);
		[window close];
		[window release];
		osHandle = nullptr;
	}

	closedFlag = true;
}

bool Window::IsClosed() const
{
	return closedFlag;
}

intptr_t Window::GetNativeHandle() const
{
	return reinterpret_cast<intptr_t>(osHandle);
}

} // namespace OS

#endif // PLATFORM_OSX
