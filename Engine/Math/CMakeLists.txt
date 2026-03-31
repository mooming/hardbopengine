cmake_minimum_required (VERSION 3.12)
project (Math)

set (CMAKE_CONFIGURATION_TYPES "Debug;Dev;Release" CACHE STRING "" FORCE)
if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif ()

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set (CMAKE_CXX_FLAGS_DEV "${CMAKE_CXX_FLAGS_DEBUG} -O1")
set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

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
add_library (Math STATIC 
 AABB.cpp
 AABB.cpp
 ImportanceSampling.cpp
 ImportanceSampling.cpp
 MathUtil.cpp
 MathUtil.cpp
 Matrix2x2.cpp
 Matrix2x2.cpp
 Matrix3x3.cpp
 Matrix3x3.cpp
 Matrix4x4.cpp
 Matrix4x4.cpp
 MonteCarloIntegrator.cpp
 MonteCarloIntegrator.cpp
 OBB.cpp
 OBB.cpp
 Quaternion.cpp
 Quaternion.cpp
 RigidTransform.cpp
 RigidTransform.cpp
 StratifiedSampling.cpp
 StratifiedSampling.cpp
 Transform.cpp
 Transform.cpp
 UniformTransform.cpp
 UniformTransform.cpp
 Vector2.cpp
 Vector2.cpp
 Vector3.cpp
 Vector3.cpp
 Vector4.cpp
 Vector4.cpp
 AABB.h
 AABB.h
 CoordinateOrientation.h
 CoordinateOrientation.h
 ImportanceResampling.h
 ImportanceResampling.h
 MathUtil.h
 MathUtil.h
 Matrix2x2.h
 Matrix2x2.h
 Matrix3x3.h
 Matrix3x3.h
 Matrix4x4.h
 Matrix4x4.h
 MatrixCommonImpl.inl
 MatrixCommonImpl.inl
 MonteCarloIntegrator.h
 MonteCarloIntegrator.h
 OBB.h
 OBB.h
 Quaternion.h
 Quaternion.h
 RigidTransform.h
 RigidTransform.h
 StratifiedSampling.h
 StratifiedSampling.h
 Transform.h
 Transform.h
 UniformTransform.h
 UniformTransform.h
 Vector2.h
 Vector2.h
 Vector3.h
 Vector3.h
 Vector4.h
 Vector4.h
 VectorCommonImpl.inl
 VectorCommonImpl.inl
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Math DESTINATION ${CMAKE_SOURCE_DIR}/lib)



