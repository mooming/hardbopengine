cmake_minimum_required (VERSION 3.12)
project (System)

set (CMAKE_CXX_STANDARD 20)

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Source
 ${CMAKE_SOURCE_DIR}/External
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_library (System STATIC 
 CommandLineArguments.cpp
 Component.cpp
 ComponentSystem.cpp
 Debug.cpp
 ScopedLock.cpp
 SystemStatistics.cpp
 Task.cpp
 TaskHandle.cpp
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
 Runnable.h
 ScopedLock.h
 ScopedTime.h
 SystemStatistics.h
 Task.h
 TaskHandle.h
 TaskStream.h
 TaskSystem.h
 Time.h
 Types.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS System DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (System OSAL)

add_dependencies (System OSAL)


