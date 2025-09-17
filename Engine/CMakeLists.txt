cmake_minimum_required (VERSION 3.12)
project (HEngineBase)

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

add_subdirectory (Config)
add_subdirectory (Container)
add_subdirectory (Core)
add_subdirectory (Engine)
add_subdirectory (HSTL)
add_subdirectory (Log)
add_subdirectory (Math)
add_subdirectory (Memory)
add_subdirectory (OSAL)
add_subdirectory (Resource)
add_subdirectory (String)
add_subdirectory (Test)




