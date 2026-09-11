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

	// Deliberately not [app terminate:]: that call ends in exit(), which tears the process
	// down from inside a destructor. The consequences are silent and severe - main()'s return
	// value is discarded, so no application can ever report a status (EngineTest could not
	// signal a failing suite), and static destructors never run. NSApplication is a
	// process-wide singleton that outlives this wrapper, and the engine owns its own shutdown
	// sequence, so stopping the run loop is all that belongs here.
	// Quitting on the close button is unaffected: that path is windowShouldClose: ->
	// shouldClose flag -> the application loop exits on its own.
	auto app = static_cast<NSApplication*>(m_platformHandle);
	[app stop:nil];

	m_platformHandle = nullptr;
}

void Application::initialize()
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

void Application::pollEvents()
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
