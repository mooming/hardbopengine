cmake_minimum_required (VERSION 3.12)
project (GraphicTest)

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


add_executable (GraphicTest
 Main.cpp
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS GraphicTest DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (GraphicTest Config Container HEngine HSTL Log Math Memory OSAL Resource String System Test)

add_dependencies (GraphicTest Config)
add_dependencies (GraphicTest Container)
add_dependencies (GraphicTest HEngine)
add_dependencies (GraphicTest HSTL)
add_dependencies (GraphicTest Log)
add_dependencies (GraphicTest Math)
add_dependencies (GraphicTest Memory)
add_dependencies (GraphicTest OSAL)
add_dependencies (GraphicTest Resource)
add_dependencies (GraphicTest String)
add_dependencies (GraphicTest System)
add_dependencies (GraphicTest Test)


