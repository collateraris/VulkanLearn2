# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("vulkan_guide")

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(third_party)

set( ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build Assimp Tools" FORCE )
set( ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Build Assimp Samples" FORCE )
set( ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build Assimp Tests" FORCE )

add_subdirectory( third_party/assimp )

set_target_properties( assimp
    PROPERTIES
        FOLDER third_party/assimp
)

add_subdirectory( third_party/NRD )

set_target_properties( NRD
    PROPERTIES
        FOLDER third_party/NRD
)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)


find_program(GLSLC "glslc.exe" $ENV{VULKAN_SDK}/Bin/)
find_program(DXC "dxc.exe" ${PROJECT_SOURCE_DIR}/third_party/dxc/)

## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/*.mesh"
    "${PROJECT_SOURCE_DIR}/shaders/*.task"
    "${PROJECT_SOURCE_DIR}/shaders/*.rgen"
    "${PROJECT_SOURCE_DIR}/shaders/*.rmiss"
    "${PROJECT_SOURCE_DIR}/shaders/*.rchit"
    )

## iterate each shader
foreach(GLSL_SRC ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL_SRC} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL_SRC})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSLC} --target-spv=spv1.6 ${GLSL_SRC} -o ${SPIRV}
    DEPENDS ${GLSL_SRC})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL_SRC)

## find all the shader files under the shaders folder
file(GLOB_RECURSE HLSL_NRD_CS_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/third_party/NRD/Shaders/Source/*.cs.hlsl"
)

## iterate each shader
foreach(HLSL_NRD_CS_SRC ${HLSL_NRD_CS_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${HLSL_NRD_CS_SRC} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/third_party/NRD/Shaders/Source/${FILE_NAME}.spv")
  message(STATUS ${HLSL_NRD_CS_SRC})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${DXC} -spirv ${HLSL_NRD_CS_SRC} -Fo ${SPIRV} -T cs_6_0 -E main -D NRD_COMPILER_DXC=1 -D NRD_NORMAL_ENCODING=2 -D NRD_ROUGHNESS_ENCODING=1
    DEPENDS ${HLSL_NRD_CS_SRC})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(HLSL_NRD_CS_SRC)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
