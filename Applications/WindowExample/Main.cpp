// Copyright (c) 2026 Hansol Park (moomning.go@gmail.com). All rights reserved.

#include "Engine/OSAL/Application.h"
#include "Engine/OSAL/Window.h"

#include <thread>
#include <chrono>

#include "Engine/Engine.h"

int main(int argc, const char* argv[])
{
	hbe::Engine hengine;
	hengine.Initialize(argc, argv);

	auto app = OS::CreateApplication();
	if (!app)
	{
		return 1;
	}

	app->Initialize();

	auto window = OS::CreateWindow("Hello? 안녕하세요?", 800, 600);
	if (!window)
	{
		return 1;
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));

	window->Close();
	hengine.ShutDown();

	return 0;
}
