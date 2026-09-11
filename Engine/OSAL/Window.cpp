// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Window.h"

#include "Application.h"
#include "Config/BuildConfig.h"

namespace OS
{

std::unique_ptr<Window> createWindow(const hbe::HString& title, int width, int height)
{
	auto window = std::make_unique<Window>();

	if (window->createWindow(title, width, height))
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

void WindowTest::prepare()
{
	addTest("Create Window", [this](auto& ls)
	{
		auto& taskSystem = Engine::get().getTaskSystem();
		std::promise<std::unique_ptr<OS::Window>> windowPromise;
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

		taskSystem.dispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			fatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::get().getApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lf;
				windowPromise.set_value(nullptr);
				return;
			}

			auto window = OS::createWindow("Test Window", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			const int width = window->getWidth();
			const int height = window->getHeight();
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

			if (!window->isVisible())
			{
				ls << "Window should be visible" << lferr;
			}

			ls << "A window has been created successfully" << lf;

			for (int i = 0; i < 100; ++i)
			{
				app->pollEvents();
				window->pollEvents();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	addTest("Set and Get Title", [this](auto& ls)
	{
		auto& taskSystem = Engine::get().getTaskSystem();
		std::promise<std::unique_ptr<OS::Window>> windowPromise;
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

		taskSystem.dispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			fatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::get().getApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lf;
				windowPromise.set_value(nullptr);
				return;
			}

			auto window = OS::createWindow("Initial Title", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			const int width = window->getWidth();
			const int height = window->getHeight();
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

			if (!window->isVisible())
			{
				ls << "Window should be visible" << lferr;
			}

			ls << "A window has been created successfully" << lf;

			bool titleSet = false;
			for (int i = 0; i < 100; ++i)
			{
				app->pollEvents();
				window->pollEvents();

				if (i == 50)
				{
					hbe::HString newTitle = "New Title";
					window->setTitle(newTitle);
					ls << "Title set to: " << newTitle.c_str() << lf;
					titleSet = true;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			if (!titleSet)
			{
				ls << "Failed to set title" << lf;
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	addTest("Set and Get Size", [this](auto& ls)
	{
		auto& taskSystem = Engine::get().getTaskSystem();
		std::promise<std::unique_ptr<OS::Window>> windowPromise;
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

		taskSystem.dispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			fatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::get().getApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lf;
				windowPromise.set_value(nullptr);
				return;
			}

			auto window = OS::createWindow("Resize Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			const int initialWidth = window->getWidth();
			const int initialHeight = window->getHeight();
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
				app->pollEvents();
				window->pollEvents();

				if (i == 50)
				{
					const int newWidth = 1024;
					const int newHeight = 768;
					window->setSize(newWidth, newHeight);
					ls << "Resizing to: " << newWidth << "x" << newHeight << lf;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			const int finalWidth = window->getWidth();
			const int finalHeight = window->getHeight();
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

	addTest("Visibility", [this](auto& ls)
	{
		auto& taskSystem = Engine::get().getTaskSystem();
		std::promise<std::unique_ptr<OS::Window>> windowPromise;
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

		taskSystem.dispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			fatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::get().getApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lf;
				windowPromise.set_value(nullptr);
				return;
			}

			auto window = OS::createWindow("Visibility Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			const bool initiallyVisible = window->isVisible();
			ls << "Initially visible: " << (initiallyVisible ? "true" : "false") << lf;

			if (!initiallyVisible)
			{
				ls << "Window should be initially visible" << lferr;
			}

			for (int i = 0; i < 100; ++i)
			{
				app->pollEvents();
				window->pollEvents();

				if (i == 25)
				{
					window->setVisible(false);
					ls << "Set visible: false" << lf;
				}
				else if (i == 50)
				{
					const bool isHidden = !window->isVisible();
					ls << "Is visible after hide: " << (isHidden ? "false" : "true") << lf;

					if (window->isVisible())
					{
						ls << "Window should be hidden after SetVisible(false)" << lferr;
					}
				}
				else if (i == 75)
				{
					window->setVisible(true);
					ls << "Set visible: true" << lf;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			const bool finalVisible = window->isVisible();
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

	addTest("Poll Events", [this](auto& ls)
	{
		auto& taskSystem = Engine::get().getTaskSystem();
		std::promise<std::unique_ptr<OS::Window>> windowPromise;
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

		taskSystem.dispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			fatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::get().getApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			auto window = OS::createWindow("Poll Events Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			ls << "Starting to poll events..." << lf;

			for (int i = 0; i < 50; ++i)
			{
				app->pollEvents();
				window->pollEvents();

				if (i % 10 == 0)
				{
					ls << "Poll iteration: " << i << lf;
				}

				if (window->getWidth() != 800)
				{
					ls << "Invalid width during poll: " << window->getWidth() << ", expected 800" << lferr;
				}

				if (window->getHeight() != 600)
				{
					ls << "Invalid height during poll: " << window->getHeight() << ", expected 600" << lferr;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			ls << "Poll events completed successfully" << lf;

			if (window->getWidth() != 800)
			{
				ls << "Invalid width after poll: " << window->getWidth() << ", expected 800" << lferr;
			}

			if (window->getHeight() != 600)
			{
				ls << "Invalid height after poll: " << window->getHeight() << ", expected 600" << lferr;
			}

			window.reset();
			windowPromise.set_value(nullptr);
		}, &userData);

		windowFuture.get();
	});

	addTest("Close Window", [this](auto& ls)
	{
		auto& taskSystem = Engine::get().getTaskSystem();
		std::promise<std::unique_ptr<OS::Window>> windowPromise;
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

		taskSystem.dispatchToMainThread([](void* userData) mutable
		{
			auto data = static_cast<Data*>(userData);
			fatalAssert(data != nullptr);

			auto& ls = data->ls;
			auto& lf = data->thisObject->lf;
			auto& lferr = data->thisObject->lferr;
			auto& windowPromise = data->windowPromise;

			auto app = Engine::get().getApplication();
			if (!app)
			{
				ls << "Failed to get an application instance" << lf;
				windowPromise.set_value(nullptr);
				return;
			}

			auto window = OS::createWindow("Close Test", 800, 600);
			if (!window)
			{
				ls << "Failed to create a window" << lferr;
				windowPromise.set_value(nullptr);
				return;
			}

			ls << "Window created, verifying initial state..." << lf;

			if (window->getWidth() != 800)
			{
				ls << "Invalid width: " << window->getWidth() << ", expected 800" << lferr;
			}

			if (window->getHeight() != 600)
			{
				ls << "Invalid height: " << window->getHeight() << ", expected 600" << lferr;
			}

			if (!window->isVisible())
			{
				ls << "Window should be visible" << lferr;
			}

			if (window->isClosed())
			{
				ls << "Window should not be closed initially" << lferr;
			}

			for (int i = 0; i < 20; ++i)
			{
				app->pollEvents();
				window->pollEvents();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			window->Close();
			ls << "Window closed, verifying IsClosed()..." << lf;

			if (!window->isClosed())
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
