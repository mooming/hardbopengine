// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Window.h"
#include "Core/CommonMacros.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>

namespace OS
{

Window::Window()
	: hwnd(nullptr), width(0), height(0), visibleFlag(true), closedFlag(false), shouldCloseFlag(false)
{
}

Window::~Window()
{
	Close();
}

bool Window::CreateWindow(const hbe::HString& title, int width, int height)
{
	Window::width = width;
	Window::height = height;

	HINSTANCE hInstance = GetModuleHandle(nullptr);
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "HardbopEngineWindowClass";

	RegisterClass(&wc);

	hwnd = CreateWindowEx(
		0,
		"HardbopEngineWindowClass",
		title.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,
		nullptr, nullptr, hInstance, this
	);

	returnValueIf(false, hwnd == nullptr);

	ShowWindow(static_cast<HWND>(hwnd), SW_SHOW);
	UpdateWindow(static_cast<HWND>(hwnd));

	return true;
}

void Window::SetTitle(const hbe::HString& title)
{
	if (hwnd)
	{
		SetWindowText(static_cast<HWND>(hwnd), title.c_str());
	}
}

void Window::SetSize(int width, int height)
{
	if (hwnd)
	{
		Window::width = width;
		Window::height = height;
		SetWindowPos(static_cast<HWND>(hwnd), nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
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
	if (hwnd)
	{
		return IsWindowVisible(static_cast<HWND>(hwnd));
	}

	return false;
}

void Window::SetVisible(bool visible)
{
	if (hwnd)
	{
		ShowWindow(static_cast<HWND>(hwnd), visible ? SW_SHOW : SW_HIDE);
		visibleFlag = visible;
	}
}

void Window::PollEvents()
{
	MSG msg;

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

intptr_t Window::GetNativeHandle() const
{
	return reinterpret_cast<intptr_t>(hwnd);
}

void Window::Close()
{
	if (hwnd)
	{
		DestroyWindow(static_cast<HWND>(hwnd));
		hwnd = nullptr;
	}

	closedFlag = true;
}

bool Window::IsClosed() const
{
	return closedFlag;
}

long Window::WindowProc(void* hwnd, unsigned int uMsg, unsigned long long wParam, long lParam)
{
	Window* pThis = nullptr;

	if (uMsg == WM_NCCREATE)
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		pThis = reinterpret_cast<Window*>(pCreate->lpCreateParams);
		SetWindowLongPtr(static_cast<HWND>(hwnd), GWLP_DLPTR, reinterpret_cast<LONG_PTR>(pThis));
	}
	else
	{
		pThis = reinterpret_cast<Window*>(GetWindowLongPtr(static_cast<HWND>(hwnd), GWLP_DLPTR));
	}

	if (pThis)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			pThis->shouldCloseFlag = true;
			PostQuitMessage(0);

			return 0;
		case WM_SIZE:
			pThis->width = LOWORD(lParam);
			pThis->height = HIWORD(lParam);

			return 0;
		}
	}

	return DefWindowProc(static_cast<HWND>(hwnd), uMsg, wParam, lParam);
}

} // namespace OS

#endif // PLATFORM_WINDOWS
