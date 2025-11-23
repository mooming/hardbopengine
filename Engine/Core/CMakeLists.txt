cmake_minimum_required (VERSION 3.12)
project (Core)

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
 Component.cpp
 ComponentSystem.cpp
 Debug.cpp
 RangedTask.cpp
 ScopedLock.cpp
 SystemStatistics.cpp
 Task.cpp
 TaskStream.cpp
 TaskSystem.cpp
 Time.cpp
 CommandLineArguments.h
 CommonUtil.h
 Component.h
 ComponentState.h
 ComponentSystem.h
 Constants.h
 Debug.h
 Exception.h
 RangedTask.h
 Runnable.h
 ScopedLock.h
 ScopedTime.h
 SystemStatistics.h
 Task.h
 TaskStream.h
 TaskSystem.h
 Time.h
 Types.h
        TaskStreamAffinity.cpp
        TaskStreamAffinity.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Core DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Core OSAL)

add_dependencies (Core OSAL)


