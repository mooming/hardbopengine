cmake_minimum_required (VERSION 3.12)
project (hardbopengine)

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

add_subdirectory (Engine)




