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
	hengine.Initialize(argc, argv);

	auto app = OS::CreateApplication();
	if (!app)
	{
		std::cerr << "Error: Failed to create application" << std::endl;
		return 1;
	}

	app->Initialize();

	auto window = OS::CreateWindow("Hello? 안녕하세요?", 800, 600);
	if (!window)
	{
		std::cerr << "Error: Failed to create window" << std::endl;
		return 1;
	}

	for (int i = 0; i < 10; ++i)
	{
		app->PollEvents();
		window->PollEvents();

		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (window->IsClosed())
		{
			break;
		}
	}

	window->Close();
	hengine.ShutDown();

	return 0;
}
