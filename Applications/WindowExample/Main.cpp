// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Engine/Engine.h"
#include "Engine/OSAL/Application.h"
#include "Engine/OSAL/Window.h"

#include <chrono>
#include <iostream>
#include <thread>

int main(int argc, const char* argv[]) noexcept
{
	hbe::Engine hengine;
	hengine.initialize(argc, argv);

	auto app = OS::createApplication();
	if (!app)
	{
		std::cerr << "Error: Failed to create application" << std::endl;
		return 1;
	}

	app->initialize();

	auto window = OS::createWindow("Hello? 안녕하세요?", 800, 600);
	if (!window)
	{
		std::cerr << "Error: Failed to create window" << std::endl;
		return 1;
	}

	for (int i = 0; i < 10; ++i)
	{
		app->pollEvents();
		window->pollEvents();

		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (window->isClosed())
		{
			break;
		}
	}

	window->Close();
	hengine.shutDown();

	return 0;
}
