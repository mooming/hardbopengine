cmake_minimum_required(VERSION 3.12)
project(MacOSApp)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configurations: Debug, Dev, Release
set(CMAKE_CONFIGURATION_TYPES "Debug;Dev;Release" CACHE STRING "" FORCE)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# Compiler flags per configuration
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_DEV "${CMAKE_CXX_FLAGS_DEV} -O1")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Treat warnings as errors on non-MSVC compilers
if(NOT MSVC)
    add_compile_options(-Wall -Werror)
endif()

# Find macOS frameworks
find_library(COCOA_LIBRARY Cocoa)
find_library(FOUNDATION_LIBRARY Foundation)

# Source files (C++ and Objective-C++)
add_executable(MacOSApp
    Source/Main.cpp
    Source/Framebuffer.cpp
    Source/Window.mm
    Source/Application.mm
    Source/UI/UIComponent.cpp
    Source/UI/TextPanel.cpp
    Source/UI/ScrollBar.cpp
    Source/UI/Button.cpp
)

# Include directories
target_include_directories(MacOSApp PRIVATE
    ${CMAKE_SOURCE_DIR}
    Include
)

# Link against macOS frameworks
target_link_libraries(MacOSApp
    ${COCOA_LIBRARY}
    ${FOUNDATION_LIBRARY}
)

# Install
install(TARGETS MacOSApp DESTINATION ${CMAKE_SOURCE_DIR}/../../bin)
