// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.


#include "OSAL/Application.h"
#include "OSAL/Window.h"

#include <thread>
#include <chrono>

int main(int argc, const char* argv[])
{
	auto app = OS::CreateApplication();
	if (app == nullptr)
	{
		return 1;
	}

	app->Initialize();
	auto window = OS::CreateWindow("WindowTest", 800, 600);
	if (window == nullptr)
	{
		delete app;
		return 1;
	}

	app->Run();

	std::this_thread::sleep_for(std::chrono::seconds(2));

	window->Close();
	delete window;
	delete app;

	return 0;
}
