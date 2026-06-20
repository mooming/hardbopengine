// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "OSXWindow.h"

#ifdef PLATFORM_OSX
#import <Cocoa/Cocoa.h>
#include <cstdio>

@interface PixelView : NSView {
    CGImageRef _image;
}
- (void)setImage:(CGImageRef)image;
@end

@implementation PixelView
- (void)setImage:(CGImageRef)image {
    if (_image) CGImageRelease(_image);
    _image = image;
    if (_image) CGImageRetain(_image);
    [self setNeedsDisplay:YES];
}
- (void)drawRect:(NSRect)dirtyRect {
    if (!_image) return;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(ctx, NSMakeRect(0, 0, [self bounds].size.width, [self bounds].size.height), _image);
}
- (void)dealloc {
    if (_image) CGImageRelease(_image);
    [super dealloc];
}
@end

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

	PixelView *pixelView = [[PixelView alloc] initWithFrame:frame];
	[window setContentView:pixelView];

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
	PixelView *pixelView = static_cast<PixelView*>([window contentView]);

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
			[pixelView setImage:image];
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
