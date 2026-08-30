// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Application.h"
#include "Core/CommonMacros.h"
#include "Config/BuildConfig.h"

#ifdef PLATFORM_OSX

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation AppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	return NO;
}
@end

namespace OS
{

Application::Application() noexcept
	: m_platformHandle(nullptr)
{
}

Application::~Application()
{
	returnIf(m_platformHandle == nullptr);

	auto app = static_cast<NSApplication*>(m_platformHandle);
	[app terminate:nil];

	m_platformHandle = nullptr;
}

void Application::Initialize()
{
	if (m_platformHandle != nullptr)
	{
		// Already initialised.
		return;
	}

	NSApplication *app = [NSApplication sharedApplication];
	[app setActivationPolicy:NSApplicationActivationPolicyRegular];

	auto delegate = [app delegate];
	if (delegate == nil)
	{
		AppDelegate *appDelegate = [[AppDelegate alloc] init];
		[app setDelegate:appDelegate];
	}

	[app finishLaunching];
	m_platformHandle = app;
}

void Application::PollEvents()
{
	if (m_platformHandle == nullptr)
	{
		// It hasn't been initialised.
		return;
	}

	auto app = static_cast<NSApplication*>(m_platformHandle);

	// Clear all the queued events.
	while (NSEvent *event = [app nextEventMatchingMask:NSEventMaskAny
										 untilDate:[NSDate distantPast]
											inMode:NSDefaultRunLoopMode
										 dequeue:YES])
	{
		[app sendEvent:event];
		[app updateWindows];
	}
}

} // namespace OS

#endif // PLATFORM_OSX
