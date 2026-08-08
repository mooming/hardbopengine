// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Application.h"

#include <Cocoa/Cocoa.h>

#include <iostream>


// ─────────────────────────────────────────────────────────────────────────────
// Objective-C++ Bridge: Application Delegate
// ─────────────────────────────────────────────────────────────────────────────

// This delegate prevents the app from terminating automatically when the
// last window is closed — we manage shutdown explicitly via the Close
// button or traffic light button.

@interface ApplicationAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation ApplicationAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
	(void)sender;
	return NO;
}

@end


// ─────────────────────────────────────────────────────────────────────────────
// Application C++ Implementation
// ─────────────────────────────────────────────────────────────────────────────

namespace hbe
{

Application::Application()
	: nsAppHandle(nullptr)
	, initializedFlag(false)
{
}

Application::~Application()
{
	if (initializedFlag)
	{
		ShutDown();
	}
}

bool Application::Initialize()
{
	if (initializedFlag)
	{
		return true;
	}

	NSApplication* app = [NSApplication sharedApplication];
	if (app == nil)
	{
		std::cerr << "Error: Application::Initialize - failed to get shared NSApplication"
		          << std::endl;
		return false;
	}

	// Regular activation policy: app appears in Dock, has menu bar
	[app setActivationPolicy:NSApplicationActivationPolicyRegular];

	// Set up our custom delegate
	if ([app delegate] == nil)
	{
		ApplicationAppDelegate* delegate = [[ApplicationAppDelegate alloc] init];
		[app setDelegate:delegate];
	}

	// Complete app startup (creates menu bar, processes initial events)
	[app finishLaunching];
	[app activateIgnoringOtherApps:YES];

	nsAppHandle = static_cast<void*>(app);
	initializedFlag = true;

	return true;
}

void Application::PollEvents() noexcept
{
	if (nsAppHandle == nullptr)
	{
		return;
	}

	NSApplication* app = static_cast<NSApplication*>(nsAppHandle);

	// Process all queued events immediately (non-blocking)
	while (NSEvent* event = [app nextEventMatchingMask:NSEventMaskAny
	                                          untilDate:[NSDate distantPast]
	                                             inMode:NSDefaultRunLoopMode
	                                          dequeue:YES])
	{
		[app sendEvent:event];
		[app updateWindows];
	}
}

void Application::ShutDown() noexcept
{
	if (nsAppHandle != nullptr)
	{
		NSApplication* app = static_cast<NSApplication*>(nsAppHandle);
		[app terminate:nil];
		nsAppHandle = nullptr;
	}

	initializedFlag = false;
}

bool Application::IsRunning() const noexcept
{
	return initializedFlag && nsAppHandle != nullptr;
}

} // namespace hbe
