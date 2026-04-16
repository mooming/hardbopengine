// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "OSXWindow.h"

#ifdef PLATFORM_OSX
#import <Cocoa/Cocoa.h>

namespace OS
{

OSXWindow::OSXWindow()
{
}

OSXWindow::~OSXWindow()
{
	Close();
}

bool OSXWindow::CreateWindow(const hbe::HString& title, int width, int height)
{
	NSRect frame = NSMakeRect(0, 0, width, height);
	NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
		backing:NSBackingStoreBuffered
		defer:NO];

	if (!window)
	{
		return false;
	}

	[window setTitle:[NSString stringWithUTF8String:title.c_str()]];
	[window setFrame:frame display:YES animate:NO];
	[window makeKeyAndOrderFront:nil];

	this->width = width;
	this->height = height;
	visibleFlag = true;
	nsWindow = window;

	return true;
}

void OSXWindow::SetTitle(const hbe::HString& title)
{
	if (nsWindow)
	{
		NSWindow* window = static_cast<NSWindow*>(nsWindow);
		[window setTitle:[NSString stringWithUTF8String:title.c_str()]];
	}
}

void OSXWindow::SetSize(int width, int height)
{
	if (nsWindow)
	{
		NSWindow* window = static_cast<NSWindow*>(nsWindow);
		NSRect frame = [window frame];
		frame.size.width = width;
		frame.size.height = height;
		[window setFrame:frame display:YES animate:NO];
		this->width = width;
		this->height = height;
	}
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
	if (nsWindow)
	{
		NSWindow* window = static_cast<NSWindow*>(nsWindow);

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
}

void OSXWindow::PollEvents()
{
	// Cocoa handles its own event loop via NSRunLoop.
	// In a real engine, we'd hook into the event queue or use NSEvent.
}

bool OSXWindow::ShouldClose() const
{
	return shouldCloseFlag;
}

void OSXWindow::Close()
{
	if (nsWindow)
	{
		NSWindow* window = static_cast<NSWindow*>(nsWindow);
		[window close];
		nsWindow = nullptr;
	}
}

} // namespace OS

#endif // PLATFORM_OSX
