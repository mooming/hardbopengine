// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "Window.h"

#ifdef __UNIT_TEST__
#include <thread>
#include <chrono>

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
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("Test Window", 800, 600);

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
#endif
	});

	AddTest("Set and Get Title", [this](auto& ls)
	{
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("Initial Title", 800, 600);

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
#endif
	});

	AddTest("Set and Get Size", [this](auto& ls)
	{
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("Resize Test", 800, 600);

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
#endif
	});

	AddTest("Visibility", [this](auto& ls)
	{
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("Visibility Test", 800, 600);

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
#endif
	});

	AddTest("Poll Events", [this](auto& ls)
	{
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("Poll Events Test", 800, 600);

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
#endif
	});

	AddTest("ShouldClose", [this](auto& ls)
	{
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("ShouldClose Test", 800, 600);

		if (window == nullptr)
		{
			ls << "Failed to create window" << lf;
			return;
		}

		ls << "Initial ShouldClose: " << (window->ShouldClose() ? "true" : "false") << lf;

		std::this_thread::sleep_for(std::chrono::seconds(1));
		window->Close();
#endif
	});

	AddTest("Close Window", [this](auto& ls)
	{
#ifdef PLATFORM_OSX
		ls << "Skipped: NSWindow must be created on the main thread" << lf;
		return;
#else
		auto window = OS::CreateWindow("Close Test", 800, 600);

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
#endif
	});
}

} // namespace hbe
#endif // __UNIT_TEST__
