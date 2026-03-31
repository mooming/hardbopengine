cmake_minimum_required (VERSION 3.12)
project (Core)

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
add_library (Core STATIC 
 CommandLineArguments.cpp
 CommandLineArguments.cpp
 Component.cpp
 Component.cpp
 ComponentSystem.cpp
 ComponentSystem.cpp
 Debug.cpp
 Debug.cpp
 RangedTask.cpp
 RangedTask.cpp
 ScopedLock.cpp
 ScopedLock.cpp
 SystemStatistics.cpp
 SystemStatistics.cpp
 Task.cpp
 Task.cpp
 TaskStream.cpp
 TaskStream.cpp
 TaskStreamAffinity.cpp
 TaskStreamAffinity.cpp
 TaskSystem.cpp
 TaskSystem.cpp
 Time.cpp
 Time.cpp
 CommandLineArguments.h
 CommandLineArguments.h
 CommonUtil.h
 CommonUtil.h
 Component.h
 Component.h
 ComponentState.h
 ComponentState.h
 ComponentSystem.h
 ComponentSystem.h
 Constants.h
 Constants.h
 Debug.h
 Debug.h
 Exception.h
 Exception.h
 RangedTask.h
 RangedTask.h
 Runnable.h
 Runnable.h
 ScopedLock.h
 ScopedLock.h
 ScopedTime.h
 ScopedTime.h
 SystemStatistics.h
 SystemStatistics.h
 Task.h
 Task.h
 TaskStream.h
 TaskStream.h
 TaskStreamAffinity.h
 TaskStreamAffinity.h
 TaskSystem.h
 TaskSystem.h
 Time.h
 Time.h
 Types.h
 Types.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Core DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Core OSAL OSAL)

add_dependencies (Core OSAL)
add_dependencies (Core OSAL)


