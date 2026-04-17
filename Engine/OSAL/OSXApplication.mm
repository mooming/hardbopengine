// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Config/BuildConfig.h"


#ifdef PLATFORM_OSX

#import "OSXApplication.h"
#import <Cocoa/Cocoa.h>


namespace OS
{

OSXApplication::OSXApplication()
	: appHandle(nullptr)
	, shouldTerminate(false)
{
}

OSXApplication::~OSXApplication()
{
    if (!appHandle)
    {
        return;
    }

    auto app = static_cast<NSApplication*>(appHandle);
    [app terminate:nil];
    [app release];
}

void OSXApplication::Initialize()
{
    NSApplication *app = [NSApplication sharedApplication];
    appHandle = app;
}

void OSXApplication::Run()
{
    if (appHandle == nullptr)
    {
        return;
    }

    auto app = static_cast<NSApplication*>(appHandle);
	[app run];
}

void OSXApplication::PollEvents()
{
}

bool OSXApplication::ShouldTerminate() const
{
	return shouldTerminate;
}

void OSXApplication::Terminate()
{
	shouldTerminate = true;
}

} // namespace OS

#endif // PLATFORM_OSX
