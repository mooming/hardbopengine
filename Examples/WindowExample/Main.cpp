// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "Core/TaskProvider.h"
#include "Core/TaskSystem.h"
#include "Engine/Engine.h"
#include "Engine/OSAL/Application.h"
#include "Engine/OSAL/Window.h"

namespace
{

constexpr std::size_t MaxFrameTasks = 10;

std::size_t RunWindowTick(void* userData, std::size_t startIndex, std::size_t endIndex) noexcept
{
	auto& ticks = *static_cast<std::atomic<unsigned>*>(userData);
	ticks.fetch_add(1, std::memory_order_relaxed);

	return endIndex - startIndex;
}

class WindowTickProvider final : public hbe::TaskProvider
{
public:
	WindowTickProvider(hbe::TaskSystem& targetSystem, hbe::TaskSystem::TIndex targetStream,
					   std::atomic<unsigned>& tickCounter) noexcept
		: TaskProvider("WindowTickProvider")
		, taskSystem(targetSystem)
		, stream(targetStream)
		, ticks(tickCounter)
	{
		frames.fill(hbe::TaskID{});
	}

	bool Produce(const hbe::TaskProduceContext& context) noexcept override
	{
		if (produced >= MaxFrameTasks || IsStopRequested())
		{
			return false;
		}

		const hbe::TaskID frame = taskSystem.CreateTask("WindowTick", RunWindowTick, &ticks);
		if (frame.IsNull())
		{
			return false;
		}

		frames[produced] = frame;
		++produced;

		hbe::Task* task = taskSystem.FindTask(frame);
		if (task == nullptr)
		{
			return false;
		}

		task->ReserveSubTasks(1);
		taskSystem.Enqueue(stream, task->GenerateSubTask(0, 1));

		return true;
	}

	[[nodiscard]] std::size_t GetProducedCount() const noexcept
	{
		return produced;
	}

	void ReleaseFrames() noexcept
	{
		for (std::size_t index = 0; index < produced; ++index)
		{
			taskSystem.ReleaseTask(frames[index]);
		}

		produced = 0;
	}

private:
	hbe::TaskSystem& taskSystem;
	hbe::TaskSystem::TIndex stream;
	std::atomic<unsigned>& ticks;
	std::array<hbe::TaskID, MaxFrameTasks> frames;
	std::size_t produced = 0;
};

void PumpProvider(hbe::TaskProvider& provider, hbe::TaskSystem::TIndex stream) noexcept
{
	provider.Produce(hbe::TaskProduceContext::ForStream(stream));
}

} // namespace

int main(int argc, const char* argv[]) noexcept
{
	hbe::Engine hengine;
	hengine.Initialize(argc, argv);

	auto* app = hengine.GetApplication();
	if (app == nullptr)
	{
		std::cerr << "Error: Failed to create application" << std::endl;
		return 1;
	}

	auto window = OS::CreateWindow("Hello? 안녕하세요?", 800, 600);
	if (!window)
	{
		std::cerr << "Error: Failed to create window" << std::endl;
		return 1;
	}

	auto& taskSystem = hengine.GetTaskSystem();
	const hbe::TaskSystem::TIndex tickStream = hbe::TaskSystem::GetIOTaskStreamIndex() + 1;

	if (!taskSystem.HasStream(tickStream))
	{
		std::cerr << "Error: this engine has no worker stream to attach a provider to" << std::endl;
		return 1;
	}

	std::atomic<unsigned> ticks{0};
	WindowTickProvider tickProvider(taskSystem, tickStream, ticks);
	tickProvider.AttachTo(tickStream);

	for (int frame = 0; frame < static_cast<int>(MaxFrameTasks); ++frame)
	{
		app->PollEvents();
		window->PollEvents();

		PumpProvider(tickProvider, tickStream);

		std::this_thread::sleep_for(std::chrono::seconds(1));

		if (window->IsClosed())
		{
			break;
		}
	}

	const std::size_t produced = tickProvider.GetProducedCount();
	for (unsigned waited = 0; ticks.load() < produced && waited < 500; ++waited)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	std::cout << "Provider produced " << produced << " frame task(s), and the stream ran " << ticks.load()
			  << " of them on stream " << tickStream << "." << std::endl;

	tickProvider.ReleaseFrames();
	tickProvider.Stop();

	window->Close();
	hengine.ShutDown();

	return 0;
}
