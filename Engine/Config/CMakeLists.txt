cmake_minimum_required (VERSION 3.12)
project (Config)

set (CMAKE_CXX_STANDARD 20)

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
add_library (Config STATIC 
 ConfigFile.cpp
 ConfigParam.cpp
 ConfigSystem.cpp
 EngineConfig.cpp
 BuildConfig.h
 ConfigFile.h
 ConfigParam.h
 ConfigSystem.h
 EngineConfig.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Config DESTINATION ${CMAKE_SOURCE_DIR}/lib)



