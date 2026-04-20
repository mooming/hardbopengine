// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Config/BuildConfig.h"


#ifdef PLATFORM_OSX

#import "OSXApplication.h"
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

OSXApplication::OSXApplication()
	: appHandle(nullptr)
{
}

OSXApplication::~OSXApplication()
{
    if (appHandle == nullptr)
    {
        return;
    }

    auto app = static_cast<NSApplication*>(appHandle);
    [app terminate:nil];

    appHandle = nullptr;
}

void OSXApplication::Initialize()
{
    if (appHandle != nullptr)
    {
        // Already initialised
        return;
    }

    NSApplication *app = [NSApplication sharedApplication];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    auto delegate = [app delegate];
    if (delegate == nil)
    {
        AppDelegate* appDelegate = [[AppDelegate alloc] init];
        [app setDelegate:appDelegate];
    }

    [app finishLaunching];
    appHandle = app;
}

void OSXApplication::PollEvents()
{
    if (appHandle == nullptr)
    {
        // It hasn't be initialised.
        return;
    }

    auto app = static_cast<NSApplication*>(appHandle);

    // Clear all the queued events
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
