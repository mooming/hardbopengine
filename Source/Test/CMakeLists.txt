cmake_minimum_required (VERSION 3.12)
project (Test)

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
 )
link_directories (${CMAKE_SOURCE_DIR}/lib)


add_library (Test STATIC 
 TestCollection.cpp
 TestEnv.cpp
 UnitTestCollection.cpp
 TestCollection.h
 TestEnv.h
 UnitTestCollection.h
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Test DESTINATION ${CMAKE_SOURCE_DIR}/lib)


target_link_libraries (Test Config Container HSTL Log Math Memory OSAL Resource String System Test)

add_dependencies (Test Config)
add_dependencies (Test Container)
add_dependencies (Test HSTL)
add_dependencies (Test Log)
add_dependencies (Test Math)
add_dependencies (Test Memory)
add_dependencies (Test OSAL)
add_dependencies (Test Resource)
add_dependencies (Test String)
add_dependencies (Test System)
add_dependencies (Test Test)


