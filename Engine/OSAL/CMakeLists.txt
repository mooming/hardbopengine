cmake_minimum_required (VERSION 3.12)
project (OSAL)

set (CMAKE_CONFIGURATION_TYPES "Debug;Dev;Release" CACHE STRING "" FORCE)
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif ()

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set (CMAKE_CXX_FLAGS_DEV "${CMAKE_CXX_FLAGS_DEBUG} -O1")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set (CMAKE_CXX_STANDARD 23)

if (APPLE)
    set(PLATFORM_SOURCES
        OSXApplication.mm
        OSXWindow.mm
    )
else()
    set(PLATFORM_SOURCES "")
endif()

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/External
 ${CMAKE_SOURCE_DIR}/Engine
 )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (OSAL STATIC 
 Application.cpp
 Directory.cpp
 LinuxAbstractLayer.cpp
 LinuxApplication.cpp
 LinuxDebug.cpp
 LinuxFileHandle.cpp
 LinuxFileOpenMode.cpp
 LinuxInputOutput.cpp
 LinuxMapSyncMode.cpp
 LinuxMemory.cpp
 LinuxProtectionMode.cpp
 LinuxThread.cpp
 LinuxWindow.cpp
 OSAbstractLayer.cpp
 OSDebug.cpp
 OSInputOutput.cpp
 OSMemory.cpp
 OSThread.cpp
 OSXAbstractLayer.cpp
 OSXDebug.cpp
 OSXFileHandle.cpp
 OSXFileOpenMode.cpp
 OSXInputOutput.cpp
 OSXMapSyncMode.cpp
 OSXMemory.cpp
 OSXProtectionMode.cpp
 OSXThread.cpp
 Win32Application.cpp
 Win32Window.cpp
 Window.cpp
 WindowsAbstractLayer.cpp
 WindowsDebug.cpp
 WindowsInputOutput.cpp
 WindowsMemory.cpp
 WindowsThread.cpp
 Application.h
 Directory.h
 File.h
 Intrinsic.h
 LinuxApplication.h
 LinuxIntrinsic.h
 LinuxWindow.h
 OSAbstractLayer.h
 OSDebug.h
 OSFileHandle.h
 OSFileOpenMode.h
 OSInputOutput.h
 OSMapSyncMode.h
 OSMemory.h
 OSProtectionMode.h
 OSThread.h
 OSXApplication.h
 OSXIntrinsic.h
 OSXWindow.h
 SourceLocation.h
 Win32Application.h
 Win32Window.h
 Window.h
 WindowsIntrinsic.h
${PLATFORM_SOURCES}
)

if (APPLE)
	include_directories ( /System/Library/Frameworks )
	set(MACOSX_BUNDLE_TRUE TRUE)
	set(MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif (APPLE)

find_library(COCOA_FRAMEWORK Cocoa)
if (COCOA_FRAMEWORK)
	target_link_libraries (OSAL ${COCOA_FRAMEWORK})
endif ()

install (TARGETS OSAL DESTINATION ${CMAKE_SOURCE_DIR}/lib)



