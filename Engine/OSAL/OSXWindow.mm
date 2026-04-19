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
{
}

OSXWindow::~OSXWindow()
{
    Close();
}

bool OSXWindow::CreateWindow(const hbe::HString& title, int width, int height)
{
    if (nsWindow != nullptr)
    {
        return false;
    }

    NSRect frame = NSMakeRect(100, 100, width, height);
    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                  styleMask:styleMask
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];

    if (window == nullptr)
    {
        return false;
    }

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
	// Cocoa handles its own event loop via NSRunLoop.
	// In a real engine, we'd hook into the event queue or use NSEvent.
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
}

} // namespace OS

#endif // PLATFORM_OSX
