// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Window.h"

#include "Application.h"
#include "Config/BuildConfig.h"

#ifdef PLATFORM_WINDOWS
#include "Win32Window.h"
#elif defined(PLATFORM_LINUX)
#include "LinuxWindow.h"
#elif defined(PLATFORM_OSX)
#include "OSXWindow.h"
#endif

namespace OS
{

std::unique_ptr<IWindow> CreateWindow(const hbe::HString& title, int width, int height)
{
#ifdef PLATFORM_WINDOWS
	auto window = std::make_unique<Win32Window>();
#elif defined(PLATFORM_LINUX)
	auto window = std::make_unique<LinuxWindow>();
#elif defined(PLATFORM_OSX)
	auto window = std::make_unique<OSXWindow>();
#else
	return nullptr;
#endif

	if (window->CreateWindow(title, width, height))
	{
		return window;
	}

	return nullptr;
}

} // namespace OS

#ifdef __UNIT_TEST__
#include <thread>
#include <chrono>
#include <future>

#include "Core/TaskSystem.h"
#include "Engine/Engine.h"

namespace hbe
{

void WindowTest::Prepare()
{
	AddTest("Create Window", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<std::unique_ptr<OS::IWindow>> windowPromise;
		auto windowFuture = windowPromise.get_future();

		using TThis = decltype(this);
		using TLs = decltype(ls);
		using TPromise = decltype(windowPromise);

		struct Data final
		{
			TThis thisObject;
			TLs& ls;
			TPromise& windowPromise;

			Data(TThis thisObj, TLs& ls, TPromise& promise)
				: thisObject(thisObj), ls(ls), windowPromise(promise)
			{
			}
		};

		Data userData{this, ls, windowPromise};

		taskSystem.DispatchToMainThread([](void* userData)
		{
			auto data = static_cast<Data*>(userData);
			FatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::Get().GetApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lf;
				return;
			}

			auto window = OS::CreateWindow("Test Window", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lf;
				windowPromise.set_value(std::move(window));
				return;
			}

			ls << "Width: " << window->GetWidth() << lf;
			ls << "Height: " << window->GetHeight() << lf;
			ls << "A window has been created successfully" << lf;

			for (int i = 0; i < 100; ++i)
			{
				app->PollEvents();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	AddTest("Set and Get Title", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<std::unique_ptr<OS::IWindow>> windowPromise;
		auto windowFuture = windowPromise.get_future();

		using TThis = decltype(this);
		using TLs = decltype(ls);
		using TPromise = decltype(windowPromise);

		struct Data final
		{
			TThis thisObject;
			TLs& ls;
			TPromise& windowPromise;

			Data(TThis thisObj, TLs& ls, TPromise& promise)
				: thisObject(thisObj), ls(ls), windowPromise(promise)
			{
			}
		};

		Data userData{this, ls, windowPromise};

		taskSystem.DispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			FatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& windowPromise = data->windowPromise;

			auto window = OS::CreateWindow("Initial Title", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lf;
				windowPromise.set_value(std::move(window));
				return;
			}

			ls << "Width: " << window->GetWidth() << lf;
			ls << "Height: " << window->GetHeight() << lf;
			ls << "A window has been created successfully" << lf;

			for (int i = 0; i < 100; ++i)
			{
				if (i == 50)
				{
					hbe::HString newTitle = "New Title";
					window->SetTitle(newTitle);
					ls << "Title set to: " << newTitle.c_str() << lf;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	AddTest("Set and Get Size", [](auto& ls)
	{

	});

	AddTest("Visibility", [](auto& ls)
	{
	});

	AddTest("Poll Events", [](auto& ls)
	{

	});

	AddTest("ShouldClose", [](auto& ls)
	{

	});

	AddTest("Close Window", [](auto& ls)
	{

	});
}

} // namespace hbe
#endif // __UNIT_TEST__
