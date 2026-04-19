// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Win32Window.h"

#ifdef PLATFORM_WINDOWS
#include <windows.h>

namespace OS
{

Win32Window::Win32Window()
{
}

Win32Window::~Win32Window()
{
	Close();
}

bool Win32Window::CreateWindow(const hbe::HString& title, int width, int height)
{
	this->width = width;
	this->height = height;

	HINSTANCE hInstance = GetModuleHandle(nullptr);
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = "HardbopEngineWindowClass";

	RegisterClass(&wc);

	hwnd = CreateWindowEx(
		0,
		"HardbopEngineWindowClass",
		title.CStr(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,
		nullptr, nullptr, hInstance, this
	);

	if (hwnd == nullptr)
	{
		return false;
	}

	ShowWindow(static_cast<HWND>(hwnd), SW_SHOW);
	UpdateWindow(static_cast<HWND>(hwnd));

	return true;
}

void Win32Window::SetTitle(const hbe::HString& title)
{
	if (hwnd)
	{
		SetWindowText(static_cast<HWND>(hwnd), title.CStr());
	}
}

void Win32Window::SetSize(int width, int height)
{
	if (hwnd)
	{
		this->width = width;
		this->height = height;
		SetWindowPos(static_cast<HWND>(hwnd), nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
	}
}

int Win32Window::GetWidth() const
{
	return width;
}

int Win32Window::GetHeight() const
{
	return height;
}

bool Win32Window::IsVisible() const
{
	if (hwnd)
	{
		return IsWindowVisible(static_cast<HWND>(hwnd));
	}

	return false;
}

void Win32Window::SetVisible(bool visible)
{
	if (hwnd)
	{
		ShowWindow(static_cast<HWND>(hwnd), visible ? SW_SHOW : SW_HIDE);
	}

	visibleFlag = visible;
}

void Win32Window::PollEvents()
{
	MSG msg;

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void Win32Window::Close()
{
	if (hwnd)
	{
		DestroyWindow(static_cast<HWND>(hwnd));
		hwnd = nullptr;
	}
}

} // namespace OS

long OS::WindowProc(void* hwnd, unsigned int uMsg, unsigned long long wParam, long lParam)
{
	Win32Window* pThis = nullptr;

	if (uMsg == WM_NCCREATE)
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		pThis = reinterpret_cast<Win32Window*>(pCreate->lpCreateParams);
		SetWindowLongPtr(static_cast<HWND>(hwnd), GWLP_DLPTR, reinterpret_cast<LONG_PTR>(pThis));
	}
	else
	{
		pThis = reinterpret_cast<Win32Window*>(GetWindowLongPtr(static_cast<HWND>(hwnd), GWLP_DLPTR));
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

#endif // PLATFORM_WINDOWS
