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

		taskSystem.DispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			FatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
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
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(std::move(window));
				return;
			}

			const int width = window->GetWidth();
			const int height = window->GetHeight();
			ls << "Width: " << width << lf;
			ls << "Height: " << height << lf;

			if (width != 800)
			{
				ls << "Invalid width: " << width << ", expected 800" << lferr;
			}

			if (height != 600)
			{
				ls << "Invalid height: " << height << ", expected 600" << lferr;
			}

			if (!window->IsVisible())
			{
				ls << "Window should be visible" << lferr;
			}

			ls << "A window has been created successfully" << lf;

			for (int i = 0; i < 100; ++i)
			{
				app->PollEvents();
				window->PollEvents();
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
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto window = OS::CreateWindow("Initial Title", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(std::move(window));
				return;
			}

			const int width = window->GetWidth();
			const int height = window->GetHeight();
			ls << "Width: " << width << lf;
			ls << "Height: " << height << lf;

			if (width != 800)
			{
				ls << "Invalid width: " << width << ", expected 800" << lferr;
			}

			if (height != 600)
			{
				ls << "Invalid height: " << height << ", expected 600" << lferr;
			}

			if (!window->IsVisible())
			{
				ls << "Window should be visible" << lferr;
			}

			ls << "A window has been created successfully" << lf;

			bool titleSet = false;
			for (int i = 0; i < 100; ++i)
			{
				if (i == 50)
				{
					hbe::HString newTitle = "New Title";
					window->SetTitle(newTitle);
					ls << "Title set to: " << newTitle.c_str() << lf;
					titleSet = true;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			if (!titleSet)
				ls << "Failed to set title" << lf;

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	AddTest("Set and Get Size", [this](auto& ls)
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
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto window = OS::CreateWindow("Resize Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(std::move(window));
				return;
			}

			const int initialWidth = window->GetWidth();
			const int initialHeight = window->GetHeight();
			ls << "Initial Size: " << initialWidth << "x" << initialHeight << lf;

			if (initialWidth != 800)
			{
				ls << "Invalid initial width: " << initialWidth << ", expected 800" << lferr;
			}
			if (initialHeight != 600)
			{
				ls << "Invalid initial height: " << initialHeight << ", expected 600" << lferr;
			}

			for (int i = 0; i < 100; ++i)
			{
				if (i == 50)
				{
					const int newWidth = 1024;
					const int newHeight = 768;
					window->SetSize(newWidth, newHeight);
					ls << "Resizing to: " << newWidth << "x" << newHeight << lf;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			const int finalWidth = window->GetWidth();
			const int finalHeight = window->GetHeight();
			ls << "Final Size: " << finalWidth << "x" << finalHeight << lf;

			if (finalWidth != 1024)
			{
				ls << "Invalid final width: " << finalWidth << ", expected 1024" << lferr;
			}
			if (finalHeight != 768)
			{
				ls << "Invalid final height: " << finalHeight << ", expected 768" << lferr;
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	AddTest("Visibility", [this](auto& ls)
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
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto window = OS::CreateWindow("Visibility Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(std::move(window));
				return;
			}

			const bool initiallyVisible = window->IsVisible();
			ls << "Initially visible: " << (initiallyVisible ? "true" : "false") << lf;

			if (!initiallyVisible)
			{
				ls << "Window should be initially visible" << lferr;
			}

			for (int i = 0; i < 100; ++i)
			{
				if (i == 25)
				{
					window->SetVisible(false);
					ls << "Set visible: false" << lf;
				}
				else if (i == 50)
				{
					const bool isHidden = !window->IsVisible();
					ls << "Is visible after hide: " << (isHidden ? "false" : "true") << lf;

					if (window->IsVisible())
					{
						ls << "Window should be hidden after SetVisible(false)" << lferr;
					}
				}
				else if (i == 75)
				{
					window->SetVisible(true);
					ls << "Set visible: true" << lf;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			const bool finalVisible = window->IsVisible();
			ls << "Final visible: " << (finalVisible ? "true" : "false") << lf;

			if (!finalVisible)
			{
				ls << "Window should be visible at the end" << lferr;
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	AddTest("Poll Events", [this](auto& ls)
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
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto window = OS::CreateWindow("Poll Events Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(std::move(window));
				return;
			}

			auto app = Engine::Get().GetApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lferr;
				window.reset();
				windowPromise.set_value(nullptr);
				return;
			}

			ls << "Starting to poll events..." << lf;

			for (int i = 0; i < 50; ++i)
			{
				app->PollEvents();
				window->PollEvents();

				if (i % 10 == 0)
				{
					ls << "Poll iteration: " << i << lf;
				}

				if (window->GetWidth() != 800)
				{
					ls << "Invalid width during poll: " << window->GetWidth() << ", expected 800" << lferr;
				}

				if (window->GetHeight() != 600)
				{
					ls << "Invalid height during poll: " << window->GetHeight() << ", expected 600" << lferr;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			ls << "Poll events completed successfully" << lf;

			if (window->GetWidth() != 800)
			{
				ls << "Invalid width after poll: " << window->GetWidth() << ", expected 800" << lferr;
			}

			if (window->GetHeight() != 600)
			{
				ls << "Invalid height after poll: " << window->GetHeight() << ", expected 600" << lferr;
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	AddTest("Close Window", [this](auto& ls)
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
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto window = OS::CreateWindow("Close Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(std::move(window));
				return;
			}

			ls << "Window created, verifying initial state..." << lf;

			if (window->GetWidth() != 800)
			{
				ls << "Invalid width: " << window->GetWidth() << ", expected 800" << lferr;
			}

			if (window->GetHeight() != 600)
			{
				ls << "Invalid height: " << window->GetHeight() << ", expected 600" << lferr;
			}

			if (!window->IsVisible())
			{
				ls << "Window should be visible" << lferr;
			}

			if (window->IsClosed())
			{
				ls << "Window should not be closed initially" << lferr;
			}

			for (int i = 0; i < 20; ++i)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			window->Close();
			ls << "Window closed, verifying IsClosed()..." << lf;

			if (!window->IsClosed())
			{
				ls << "Window should be closed after Close()" << lferr;
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
