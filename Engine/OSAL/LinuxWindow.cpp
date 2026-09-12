// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Window.h"
#include "Core/CommonMacros.h"

#ifdef PLATFORM_LINUX

namespace OS
{

Window::Window()
	: display(nullptr), window(0), width(0), height(0), visibleFlag(true), closedFlag(false)
{
}

Window::~Window()
{
	Close();
}

bool Window::CreateWindow(const hbe::HString& title, int width, int height)
{
	display = XOpenDisplay(nullptr);

	returnValueIf(false, !display);

	window = XCreateSimpleWindow(
		display,
		DefaultRootWindow(display),
		0, 0, width, height,
		1,
		BlackPixel(display, DefaultScreen(display)),
		WhitePixel(display, DefaultScreen(display))
	);

	returnValueIf(false, !window);

	Window::width = width;
	Window::height = height;

	XStoreName(display, window, title.c_str());
	XMapWindow(display, window);

	return true;
}

void Window::SetTitle(const hbe::HString& title)
{
	if (display && window)
	{
		XStoreName(display, window, title.c_str());
	}
}

void Window::SetSize(int width, int height)
{
	if (display && window)
	{
		Window::width = width;
		Window::height = height;
		XResizeWindow(display, window, width, height);
	}
}

int Window::GetWidth() const
{
	return width;
}

int Window::GetHeight() const
{
	return height;
}

bool Window::IsVisible() const
{
	return visibleFlag;
}

intptr_t Window::GetNativeHandle() const
{
	return reinterpret_cast<intptr_t>(window);
}

void Window::PollEvents()
{
	returnIf(!display);

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

void Window::Close()
{
	if (display && window)
	{
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		display = nullptr;
		window = 0;
	}

	closedFlag = true;
}

bool Window::IsClosed() const
{
	return closedFlag;
}

} // namespace OS

#endif // PLATFORM_LINUX
