cmake_minimum_required (VERSION 3.12)
project (hengine)

set (CMAKE_CXX_STANDARD 20)

if (MSVC)
	add_compile_options (/W4 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Source
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)

add_subdirectory (Memory)
add_subdirectory (Test)
add_subdirectory (Config)
add_subdirectory (OSAL)
add_subdirectory (Math)
add_subdirectory (Container)
add_subdirectory (System)
add_subdirectory (Log)
add_subdirectory (String)
add_subdirectory (HSTL)

add_executable (hengine
 Engine.cpp
 Main.cpp
 Engine.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS hengine DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (hengine Config Container HSTL Log Math Memory OSAL String System Test)

add_dependencies (hengine Config)
add_dependencies (hengine Container)
add_dependencies (hengine HSTL)
add_dependencies (hengine Log)
add_dependencies (hengine Math)
add_dependencies (hengine Memory)
add_dependencies (hengine OSAL)
add_dependencies (hengine String)
add_dependencies (hengine System)
add_dependencies (hengine Test)


