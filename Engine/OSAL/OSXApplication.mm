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
	: platformHandle(nullptr)
{
}

Application::~Application()
{
	returnIf(platformHandle == nullptr);

	auto app = static_cast<NSApplication*>(platformHandle);
	[app stop:nil];

	platformHandle = nullptr;
}

void Application::Initialize()
{
	if (platformHandle != nullptr)
	{
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
	platformHandle = app;
}

void Application::PollEvents()
{
	if (platformHandle == nullptr)
	{
		return;
	}

	auto app = static_cast<NSApplication*>(platformHandle);

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
