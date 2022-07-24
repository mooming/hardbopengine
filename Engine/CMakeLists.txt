cmake_minimum_required (VERSION 3.12)
project (hengine)

set (CMAKE_CXX_STANDARD 17)

if (MSVC)
	add_compile_options (/W4 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Engine
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)

add_subdirectory (Test)
add_subdirectory (Math)
add_subdirectory (System)

add_executable (hengine
 Main.cpp
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS hengine DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (hengine Math System)

add_dependencies (hengine Math)
add_dependencies (hengine System)


