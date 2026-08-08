// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <memory>

namespace hbe
{

/**
 * @brief Manages the macOS application lifecycle (NSApplication).
 *
 * This class is a C++ wrapper around NSApplication, providing a simple
 * interface for initialization, event polling, and shutdown. It replaces
 * the engine's OS::IApplication abstraction as a learning exercise.
 *
 * The implementation lives in Application.mm (Objective-C++).
 */
class Application final
{
public:
	Application();
	~Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) noexcept = delete;
	Application& operator=(Application&&) noexcept = delete;

	/**
	 * @brief Initializes the NSApplication singleton.
	 * Must be called before creating windows.
	 * @return True if initialization succeeded, false otherwise.
	 */
	bool Initialize();

	/**
	 * @brief Processes all pending OS events from the event queue.
	 * Should be called once per frame in the main loop.
	 */
	void PollEvents() noexcept;

	/**
	 * @brief Shuts down the application and releases native resources.
	 */
	void ShutDown() noexcept;

	/**
	 * @brief Checks if the application is still running.
	 * @return True if the application should continue running.
	 */
	[[nodiscard]] bool IsRunning() const noexcept;

private:
	void* nsAppHandle;
	bool initializedFlag;
};

} // namespace hbe
