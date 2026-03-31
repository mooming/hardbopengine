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
link_directories (${CMAKE_SOURCE_DIR}/lib)


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
add_library (OSAL STATIC 
 Directory.cpp
 Directory.cpp
 LinuxAbstractLayer.cpp
 LinuxAbstractLayer.cpp
 LinuxDebug.cpp
 LinuxDebug.cpp
 LinuxFileHandle.cpp
 LinuxFileHandle.cpp
 LinuxFileOpenMode.cpp
 LinuxFileOpenMode.cpp
 LinuxInputOutput.cpp
 LinuxInputOutput.cpp
 LinuxMapSyncMode.cpp
 LinuxMapSyncMode.cpp
 LinuxMemory.cpp
 LinuxMemory.cpp
 LinuxProtectionMode.cpp
 LinuxProtectionMode.cpp
 LinuxThread.cpp
 LinuxThread.cpp
 OSAbstractLayer.cpp
 OSAbstractLayer.cpp
 OSDebug.cpp
 OSDebug.cpp
 OSInputOutput.cpp
 OSInputOutput.cpp
 OSMemory.cpp
 OSMemory.cpp
 OSThread.cpp
 OSThread.cpp
 OSXAbstractLayer.cpp
 OSXAbstractLayer.cpp
 OSXDebug.cpp
 OSXDebug.cpp
 OSXFileHandle.cpp
 OSXFileHandle.cpp
 OSXFileOpenMode.cpp
 OSXFileOpenMode.cpp
 OSXInputOutput.cpp
 OSXInputOutput.cpp
 OSXMapSyncMode.cpp
 OSXMapSyncMode.cpp
 OSXMemory.cpp
 OSXMemory.cpp
 OSXProtectionMode.cpp
 OSXProtectionMode.cpp
 OSXThread.cpp
 OSXThread.cpp
 WindowsAbstractLayer.cpp
 WindowsAbstractLayer.cpp
 WindowsDebug.cpp
 WindowsDebug.cpp
 WindowsInputOutput.cpp
 WindowsInputOutput.cpp
 WindowsMemory.cpp
 WindowsMemory.cpp
 WindowsThread.cpp
 WindowsThread.cpp
 Directory.h
 Directory.h
 File.h
 File.h
 Intrinsic.h
 Intrinsic.h
 LinuxIntrinsic.h
 LinuxIntrinsic.h
 OSAbstractLayer.h
 OSAbstractLayer.h
 OSDebug.h
 OSDebug.h
 OSFileHandle.h
 OSFileHandle.h
 OSFileOpenMode.h
 OSFileOpenMode.h
 OSInputOutput.h
 OSInputOutput.h
 OSMapSyncMode.h
 OSMapSyncMode.h
 OSMemory.h
 OSMemory.h
 OSProtectionMode.h
 OSProtectionMode.h
 OSThread.h
 OSThread.h
 OSXIntrinsic.h
 OSXIntrinsic.h
 SourceLocation.h
 SourceLocation.h
 WindowsIntrinsic.h
 WindowsIntrinsic.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS OSAL DESTINATION ${CMAKE_SOURCE_DIR}/lib)



