// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Core/CommonMacros.h"
#include "Window.h"

#if PLATFORM_LINUX
#include <X11/Xlib.h>

namespace OS
{
Window::Window() noexcept
	: width(0)
	, height(0)
	, visibleFlag(true)
	, closedFlag(false)
	, display(nullptr)
	, window(0)
{
}

Window::~Window()
{
	Close();
}

bool Window::CreateWindow(const hbe::HString& title, int width, int height)
{
	display = XOpenDisplay(nullptr);

	ReturnValueIf(false, !display);

	window = XCreateSimpleWindow(
			static_cast<Display*>(display), DefaultRootWindow(static_cast<Display*>(display)), 0, 0, width, height, 1,
			BlackPixel(static_cast<Display*>(display), DefaultScreen(static_cast<Display*>(display))),
			WhitePixel(static_cast<Display*>(display), DefaultScreen(static_cast<Display*>(display))));

	ReturnValueIf(false, !window);

	Window::width = width;
	Window::height = height;

	XStoreName(static_cast<Display*>(display), window, title.c_str());
	XMapWindow(static_cast<Display*>(display), window);

	return true;
}

void Window::SetTitle(const hbe::HString& title)
{
	if (display && window)
	{
		XStoreName(static_cast<Display*>(display), window, title.c_str());
	}
}

void Window::SetSize(int width, int height)
{
	if (display && window)
	{
		Window::width = width;
		Window::height = height;
		XResizeWindow(static_cast<Display*>(display), window, width, height);
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

void Window::SetVisible(bool visible)
{
	if (display != nullptr && window != 0)
	{
		if (visible)
		{
			XMapWindow(static_cast<Display*>(display), window);
		}
		else
		{
			XUnmapWindow(static_cast<Display*>(display), window);
		}
	}

	visibleFlag = visible;
}

intptr_t Window::GetNativeHandle() const
{
	return reinterpret_cast<intptr_t>(window);
}

void Window::PollEvents()
{
	ReturnIf(!display);

	while (XPending(static_cast<Display*>(display)))
	{
		XEvent event;
		XNextEvent(static_cast<Display*>(display), &event);

		if (event.type == ClientMessage)
		{
		}
		else if (event.type == ConfigureNotify)
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
		XDestroyWindow(static_cast<Display*>(display), window);
		XCloseDisplay(static_cast<Display*>(display));
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
