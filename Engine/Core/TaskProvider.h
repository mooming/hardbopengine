// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

#include "Core/StreamDrainPolicy.h"
#include "Core/TaskStreamIndex.h"
#include "Core/Time.h"
#include "Core/WorkItem.h"
#include "String/StaticString.h"


namespace hbe
{
class TaskProvider;
class TaskSystem;

/// API reference: docs/Core/TaskHandle/index.html
class TaskHandle final
{
private:
	TaskProvider* provider = nullptr;

public:
	TaskHandle() = default;
	explicit TaskHandle(TaskProvider& provider) noexcept;

	[[nodiscard]] explicit operator bool() const noexcept
	{
		return provider != nullptr;
	}

	[[nodiscard]] TaskProvider* GetProvider() const noexcept
	{
		return provider;
	}

	[[nodiscard]] bool IsStopRequested() const noexcept;
	void RequestStop() const noexcept;
};

/// API reference: docs/Core/TaskProduceContext/index.html
struct TaskProduceContext final
{
public:
	TStreamIndex stream = 0;
	time::TEngineTimePoint now{};

	[[nodiscard]] static TaskProduceContext ForStream(TStreamIndex streamIndex) noexcept;
};

/// API reference: docs/Core/TaskProvider/index.html
class TaskProvider
{
public:
	static constexpr TStreamIndex MaxAttachedStreams = 64;

	static constexpr std::uint8_t LaneBitFifo = 1U << 0;
	static constexpr std::uint8_t LaneBitPriority = 1U << 1;

private:
	StaticString name;
	TaskSystem& taskSystem;

	std::array<TStreamIndex, static_cast<size_t>(MaxAttachedStreams)> attached{};
	std::array<std::uint8_t, static_cast<size_t>(MaxAttachedStreams)> lanes{};
	TStreamIndex attachedCount = 0;

	int registeredCount = 0;

	std::atomic<bool> stopRequested{false};

public:
	[[nodiscard]] static constexpr std::uint8_t LaneBit(StreamDrainPolicy::ELane lane) noexcept
	{
		return lane == StreamDrainPolicy::ELane::Fifo
					   ? LaneBitFifo
					   : (lane == StreamDrainPolicy::ELane::Priority ? LaneBitPriority : 0U);
	}

	explicit TaskProvider(StaticString name, TaskSystem& targetSystem) noexcept;
	virtual ~TaskProvider();
	TaskProvider(const TaskProvider&) = delete;
	TaskProvider& operator=(const TaskProvider&) = delete;

	virtual std::optional<WorkItem> Produce(const TaskProduceContext& context) noexcept = 0;

	void AttachTo(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept;
	void DetachFrom(TStreamIndex stream) noexcept;
	void DetachAll() noexcept;

	[[nodiscard]] TStreamIndex GetAttachedCount() const noexcept
	{
		return attachedCount;
	}

	[[nodiscard]] TStreamIndex GetAttachedStream(TStreamIndex index) const noexcept;
	[[nodiscard]] bool IsAttachedTo(TStreamIndex stream) const noexcept;
	[[nodiscard]] std::uint8_t GetAttachedLanes(TStreamIndex stream) const noexcept;

	void Stop() noexcept;

	[[nodiscard]] bool IsStopRequested() const noexcept
	{
		return stopRequested.load(std::memory_order_acquire);
	}

	[[nodiscard]] TaskHandle GetHandle() noexcept;

	[[nodiscard]] StaticString GetName() const noexcept
	{
		return name;
	}

protected:
	static WorkItem MakeWholeItem(Task& task, uint8_t priority = 0) noexcept;

private:
	void RegisterOnStream(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept;
	void UnregisterFromStream(TStreamIndex stream, StreamDrainPolicy::ELane lane) noexcept;
};
} // namespace hbe

#ifdef __TEST__
#include "Engine/Engine.h"
#include "Test/TestCollection.h"

namespace hbe
{
class TaskProviderTest final : public TestCollection
{
public:
	TaskProviderTest()
		: TestCollection("TaskProviderTest")
	{
	}

protected:
	[[nodiscard]] static hbe::TaskSystem& System() noexcept
	{
		return Engine::Get().GetTaskSystem();
	}

	void Prepare() override;
};
} // namespace hbe
#endif //__TEST__
