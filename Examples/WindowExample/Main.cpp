// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <optional>
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
	WindowTickProvider(hbe::TaskSystem& targetSystem, std::atomic<unsigned>& tickCounter) noexcept
		: TaskProvider("WindowTickProvider", targetSystem)
		, taskSystem(targetSystem)
		, ticks(tickCounter)
	{
		frames.fill(hbe::TaskID{});
	}

	std::optional<hbe::WorkItem> Produce(const hbe::TaskProduceContext& context) noexcept override
	{
		if (produced >= MaxFrameTasks || IsStopRequested())
		{
			return std::nullopt;
		}

		const hbe::TaskID frame = taskSystem.CreateTask("WindowTick", RunWindowTick, &ticks);
		if (frame.IsNull())
		{
			return std::nullopt;
		}

		frames[produced] = frame;
		++produced;

		hbe::Task* task = taskSystem.FindTask(frame);
		if (task == nullptr)
		{
			return std::nullopt;
		}

		return MakeWholeItem(*task);
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
	std::atomic<unsigned>& ticks;
	std::array<hbe::TaskID, MaxFrameTasks> frames;
	std::size_t produced = 0;
};

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
	WindowTickProvider tickProvider(taskSystem, ticks);
	tickProvider.AttachTo(tickStream, hbe::StreamDrainPolicy::ELane::Fifo);

	for (int frame = 0; frame < static_cast<int>(MaxFrameTasks); ++frame)
	{
		app->PollEvents();
		window->PollEvents();

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
	tickProvider.DetachAll();

	window->Close();
	hengine.ShutDown();

	return 0;
}
