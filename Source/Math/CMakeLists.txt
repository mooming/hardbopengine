cmake_minimum_required (VERSION 3.12)
project (Math)

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


add_library (Math STATIC 
 AABB.cpp
 ImportanceSampling.cpp
 MathUtil.cpp
 Matrix2x2.cpp
 Matrix3x3.cpp
 Matrix4x4.cpp
 MonteCarloIntegrator.cpp
 OBB.cpp
 Quaternion.cpp
 RigidTransform.cpp
 StratifiedSampling.cpp
 Transform.cpp
 UniformTransform.cpp
 Vector2.cpp
 Vector3.cpp
 Vector4.cpp
 AABB.h
 CoordinateOrientation.h
 ImportanceResampling.h
 MathUtil.h
 Matrix2x2.h
 Matrix3x3.h
 Matrix4x4.h
 MatrixCommonImpl.inl
 MonteCarloIntegrator.h
 OBB.h
 Quaternion.h
 RigidTransform.h
 StratifiedSampling.h
 Transform.h
 UniformTransform.h
 Vector2.h
 Vector3.h
 Vector4.h
 VectorCommonImpl.inl
)

if (APPLE)
include_directories ( /System/Library/Frameworks )
endif (APPLE)

install (TARGETS Math DESTINATION ${CMAKE_SOURCE_DIR}/lib)



