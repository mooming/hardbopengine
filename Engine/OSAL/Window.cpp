// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Window.h"

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

IWindow* CreateWindow(const hbe::HString& title, int width, int height)
{
#ifdef PLATFORM_WINDOWS
	auto window = new Win32Window();
#elif defined(PLATFORM_LINUX)
	auto window = new LinuxWindow();
#elif defined(PLATFORM_OSX)
	auto window = new OSXWindow();
#else
	return nullptr;
#endif

	if (window->CreateWindow(title, width, height))
	{
		return window;
	}

	delete window;

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

WindowTest::WindowTest()
	: TestCollection("WindowTest")
{
}

void WindowTest::Prepare()
{
	AddTest("Create Window", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("Test Window", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Window created successfully" << lf;
		ls << "Width: " << window->GetWidth() << lf;
		ls << "Height: " << window->GetHeight() << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
		delete window;
	});

	AddTest("Set and Get Title", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("Initial Title", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		hbe::HString newTitle = "New Title";
		window->SetTitle(newTitle);
		ls << "Title set to: " << newTitle.c_str() << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
		delete window;
	});

	AddTest("Set and Get Size", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("Resize Test", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Initial size: " << window->GetWidth() << "x" << window->GetHeight() << lf;

		window->SetSize(1024, 768);
		ls << "Resized to: " << window->GetWidth() << "x" << window->GetHeight() << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
	});

	AddTest("Visibility", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("Visibility Test", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Initial visibility: " << (window->IsVisible() ? "visible" : "hidden") << lf;

		window->SetVisible(false);
		ls << "After SetVisible(false): " << (window->IsVisible() ? "visible" : "hidden") << lf;

		window->SetVisible(true);
		ls << "After SetVisible(true): " << (window->IsVisible() ? "visible" : "hidden") << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
	});

	AddTest("Poll Events", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("Poll Events Test", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Polling events..." << lf;
		window->PollEvents();
		ls << "PollEvents completed without errors" << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
	});

	AddTest("ShouldClose", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("ShouldClose Test", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Initial ShouldClose: " << (window->ShouldClose() ? "true" : "false") << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
	});

	AddTest("Close Window", [this](auto& ls)
	{
		auto& taskSystem = Engine::Get().GetTaskSystem();
		std::promise<OS::IWindow*> windowPromise;
		auto windowFuture = windowPromise.get_future();

		taskSystem.DispatchToMainThread([&windowPromise]()
		{
			auto window = OS::CreateWindow("Close Test", 800, 600);
			windowPromise.set_value(window);
		});

		auto window = windowFuture.get();
		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Closing window..." << lf;
		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
		delete window;
		ls << "Window closed and deleted successfully" << lf;
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
