cmake_minimum_required (VERSION 3.12)
project (EngineTest)

set (CMAKE_CXX_STANDARD 20)

if (MSVC)
	add_compile_options (/W3 /WX)
else (MSVC)
	add_compile_options (-Wall -Werror)
endif (MSVC)

add_compile_definitions (__TEST__ __UNIT_TEST__ __UNIT_TEST__ )
include_directories ( ${CMAKE_SOURCE_DIR}
 ${CMAKE_SOURCE_DIR}/include
 ${CMAKE_SOURCE_DIR}/Source
 ${CMAKE_SOURCE_DIR}/External
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_executable (EngineTest
 TestMain.cpp
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS EngineTest DESTINATION ${CMAKE_SOURCE_DIR}/bin)


target_link_libraries (EngineTest Config Container Core HEngine HSTL Log Math Memory OSAL Resource String Test glfw)

add_dependencies (EngineTest Config)
add_dependencies (EngineTest Container)
add_dependencies (EngineTest Core)
add_dependencies (EngineTest HEngine)
add_dependencies (EngineTest HSTL)
add_dependencies (EngineTest Log)
add_dependencies (EngineTest Math)
add_dependencies (EngineTest Memory)
add_dependencies (EngineTest OSAL)
add_dependencies (EngineTest Resource)
add_dependencies (EngineTest String)
add_dependencies (EngineTest Test)
add_dependencies (EngineTest glfw)


