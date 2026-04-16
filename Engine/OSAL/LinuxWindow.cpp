// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "LinuxWindow.h"

#ifdef PLATFORM_LINUX

namespace OS
{

LinuxWindow::LinuxWindow()
{
}

LinuxWindow::~LinuxWindow()
{
	Close();
}

bool LinuxWindow::CreateWindow(const hbe::HString& title, int width, int height)
{
	display = XOpenDisplay(nullptr);

	if (!display)
	{
		return false;
	}

	window = XCreateSimpleWindow(
		display,
		DefaultRootWindow(display),
		0, 0, width, height,
		1,
		BlackPixel(display, DefaultScreen(display)),
		WhitePixel(display, DefaultScreen(display))
	);

	if (!window)
	{
		return false;
	}

	this->width = width;
	this->height = height;

	XStoreName(display, window, title.CStr());
	XMapWindow(display, window);

	return true;
}

void LinuxWindow::SetTitle(const hbe::HString& title)
{
	if (display && window)
	{
		XStoreName(display, window, title.CStr());
	}
}

void LinuxWindow::SetSize(int width, int height)
{
	if (display && window)
	{
		this->width = width;
		this->height = height;
		XResizeSubpixWindow(display, window, width, height);
	}
}

int LinuxWindow::GetWidth() const
{
	return width;
}

int LinuxWindow::GetHeight() const
{
	return height;
}

bool LinuxWindow::IsVisible() const
{
	return visibleFlag;
}

void LinuxWindow::PollEvents()
{
	if (!display)
	{
		return;
	}

	while (XPending(display))
	{
		XEvent event;
		XNextEvent(display, &event);

		if (event.type == ClientMessage)
		{
			// Simplified: check for close message if implemented via XCloseNotify or similar
		}
		else if (event.type == ConfigureEvent)
		{
			width = event.xconfigure.width;
			height = event.xconfigure.height;
		}
	}
}

bool LinuxWindow::ShouldClose() const
{
	return shouldCloseFlag;
}

void LinuxWindow::Close()
{
	if (display && window)
	{
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		display = nullptr;
		window = 0;
	}
}

} // namespace OS

#endif // PLATFORM_LINUX
