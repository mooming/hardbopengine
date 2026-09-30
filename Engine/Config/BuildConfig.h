// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once


#ifdef __linux__
#define PLATFORM_LINUX 1
#elif defined __APPLE__
#define PLATFORM_OSX 1
#elif defined _WIN32
#define PLATFORM_WINDOWS 1
#endif

#if !defined(PLATFORM_LINUX) && !defined(PLATFORM_OSX) && !defined(PLATFORM_WINDOWS)
static_assert(false, "No platform defined. Please define PLATFORM_LINUX, PLATFORM_OSX, or PLATFORM_WINDOWS.");
#endif

#define MAX_NUM_TASK_STREAMS 64


#define ENGINE_MIN_HARDWARE_THREADS 4

#define ENGINE_LOG_ENABLED 1
#define ENGINE_PARAM_DESC_ENABLED 1

#define MEMORY_VERIFICATION_ENABLED 0
#define MEMORY_LOGGING_ENABLED 0
#define MEMORY_INVESTIGATION_ENABLED 0
#define MEMORY_DANGLING_POINTER_CHECK_ENABLED 0
#define MEMORY_BUFFER_UNDERRUN_CHECK_ENABLED 0
#define FORCE_USE_SYSTEM_MALLOC 0
#define MULTIPOOL_ALLOC_LOG ".multiPoolConfig.dat"

#define LOG_ENABLED 1
#define LOG_BREAK_IF_WARNING 0
#define LOG_BREAK_IF_ERROR 0
#define LOG_FORCE_PRINT_IMMEDIATELY 0

#define PROFILE_ENABLED 0

#define RIGHT_HANDED_COORDINATE

#define MEMORY_INVESTIGATOR_TEST_ENABLED 0

#if __has_include("vulkan/vulkan.h") ||                                                                                \
				  __has_include("/opt/homebrew/include/vulkan/vulkan.h") ||                                            \
								__has_include("/usr/local/include/vulkan/vulkan.h") ||                                 \
											  __has_include("External/VulkanSDK/include/vulkan/vulkan.h")
#define VULKAN_SDK 1
#else
#define VULKAN_SDK 0
#pragma message                                                                                                        \
		"Vulkan SDK not found. Please install it using the appropriate script: ./scripts/install_sdk_macos.sh for macOS, ./scripts/install_sdk_linux.sh for Linux, or ./scripts/install_sdk_windows.bat for Windows."
#endif
