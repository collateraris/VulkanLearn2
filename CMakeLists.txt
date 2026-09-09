# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.22)

project ("vulkan_guide")

set(CMAKE_CXX_STANDARD 20)

option(RESTIR_BUILD_TESTS "Build backend-independent render graph tests" ON)
if(RESTIR_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(third_party)

set( ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Build Assimp Tools" FORCE )
set( ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Build Assimp Samples" FORCE )
set( ASSIMP_BUILD_TESTS OFF CACHE BOOL "Build Assimp Tests" FORCE )

add_subdirectory( third_party/assimp )

set_target_properties( assimp
    PROPERTIES
        FOLDER third_party/assimp
)

# This renderer consumes NRD's embedded Vulkan bytecode. Keep the encoder
# settings identical to the helpers included by the signal preparation shaders.
set(NRD_EMBEDS_SPIRV_SHADERS ON CACHE BOOL "Embed NRD Vulkan shaders" FORCE)
set(NRD_EMBEDS_DXIL_SHADERS OFF CACHE BOOL "Embed NRD D3D12 shaders" FORCE)
set(NRD_EMBEDS_DXBC_SHADERS OFF CACHE BOOL "Embed NRD D3D11 shaders" FORCE)
set(NRD_NRI OFF CACHE BOOL "Use the renderer's RHI integration" FORCE)
set(NRD_NORMAL_ENCODING "2" CACHE STRING "NRD packed normal encoding" FORCE)
set(NRD_ROUGHNESS_ENCODING "1" CACHE STRING "NRD linear roughness encoding" FORCE)
# ShaderMake needs only the DXC shipped with the installed Vulkan SDK.
set(SHADERMAKE_FIND_DXC OFF CACHE BOOL "Download D3D DXC" FORCE)
set(SHADERMAKE_FIND_FXC OFF CACHE BOOL "Find D3D FXC" FORCE)
set(SHADERMAKE_FIND_DXC_VK ON CACHE BOOL "Find Vulkan SDK DXC" FORCE)
set(SHADERMAKE_FIND_COMPILERS ON CACHE BOOL "Find shader compilers" FORCE)
add_subdirectory( third_party/NRD )

set_target_properties( NRD
    PROPERTIES
        FOLDER third_party/NRD
)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)


find_program(GLSLC "glslc.exe" $ENV{VULKAN_SDK}/Bin/)
find_program(DXC "dxc.exe" ${PROJECT_SOURCE_DIR}/third_party/dxc/)
find_program(SLANG "slangc.exe" ${VULKAN_SDK}/Include/slang)

# Shared declarations affect the shader ABI even when the entry point is unchanged.
file(GLOB GLSL_HEADER_FILES "${PROJECT_SOURCE_DIR}/shaders/*.h")
file(GLOB SLANG_DEPENDENCY_FILES
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.h"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.slang")
list(APPEND SLANG_DEPENDENCY_FILES
    "${PROJECT_SOURCE_DIR}/third_party/NRD/Shaders/NRD.hlsli"
    "${PROJECT_SOURCE_DIR}/third_party/NRD/Shaders/NRDConfig.hlsli")

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
    DEPENDS ${GLSL_SRC} ${GLSL_HEADER_FILES}
    VERBATIM)
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL_SRC)

file(GLOB_RECURSE SLANG_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.frag.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.vert.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.comp.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.mesh.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.task.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.rgen.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.rmiss.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.rchit.slang"
    "${PROJECT_SOURCE_DIR}/shaders_slang/*.rahit.slang"
    )

## iterate each shader
foreach(SLANG_SRC ${SLANG_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${SLANG_SRC} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders_slang/${FILE_NAME}.spv")
  set(GLSL "${PROJECT_SOURCE_DIR}/shaders_slang/${FILE_NAME}.glsl")
  message(STATUS ${SLANG_SRC})
  ##execute glslang command to compile that specific shader
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${SLANG}  ${SLANG_SRC} -profile glsl_460 -capability spvCooperativeVectorNV -target spirv -entry main -o ${SPIRV}
    DEPENDS ${SLANG_SRC} ${SLANG_DEPENDENCY_FILES}
    VERBATIM)
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(SLANG_SRC)    

# NRD shaders are compiled and embedded by the NRDShaders dependency.

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
