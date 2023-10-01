find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)

add_library(stb_image INTERFACE)
add_library(spirv_reflect STATIC)

add_library(tinyobjloader STATIC)

target_sources(vkbootstrap PRIVATE 
    vkbootstrap/VkBootstrap.h
    vkbootstrap/VkBootstrap.cpp
    )

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)
target_include_directories(glm INTERFACE glm)

target_sources(tinyobjloader PRIVATE 
    tinyobjloader/tiny_obj_loader.h
    tinyobjloader/tiny_obj_loader.cc
    )

target_include_directories(tinyobjloader PUBLIC tinyobjloader)


add_library(sdl2 INTERFACE)
set(sdl2_DIR "SDL_PATH" CACHE PATH "Path to SDL2")

if (WIN32)
target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include ${sdl2_image_DIR}/include)
target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib/x64 ${sdl2_image_DIR}/lib/x64)
elseif (UNIX)
target_include_directories(sdl2 INTERFACE ${sdl2_DIR}/include/SDL2 ${sdl2_image_DIR}/include/SDL2)
target_link_directories(sdl2 INTERFACE ${sdl2_DIR}/lib ${sdl2_image_DIR}/lib)
endif()

target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(spirv_reflect PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/spirv_reflect.c"
)

target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect" )
target_include_directories(spirv_reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/spv_reflect/include" )


target_sources(imgui PRIVATE
    imgui/imconfig.h	 
    imgui/imgui.h
    imgui/imgui.cpp

    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp

    imgui/imgui_internal.h
imgui/imgui_tables.cpp
imgui/imstb_rectpack.h
imgui/imstb_textedit.h
imgui/imstb_truetype.h
    imgui/imgui_impl_vulkan.h
    imgui/imgui_impl_sdl.h
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_impl_sdl.cpp
    )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)

target_include_directories(stb_image INTERFACE stb_image)

set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)

add_library(meshoptimizer STATIC)
target_include_directories(imgui PUBLIC meshoptimizer/src)
target_include_directories(imgui PUBLIC meshoptimizer/demo)
target_sources(meshoptimizer PUBLIC
    meshoptimizer/src/meshoptimizer.h
    meshoptimizer/src/allocator.cpp
    meshoptimizer/src/clusterizer.cpp
    meshoptimizer/src/indexcodec.cpp
    meshoptimizer/src/indexgenerator.cpp
    meshoptimizer/src/overdrawanalyzer.cpp
    meshoptimizer/src/overdrawoptimizer.cpp
    meshoptimizer/src/quantization.cpp
    meshoptimizer/src/simplifier.cpp
    meshoptimizer/src/spatialorder.cpp
    meshoptimizer/src/stripifier.cpp
    meshoptimizer/src/vcacheanalyzer.cpp
    meshoptimizer/src/vcacheoptimizer.cpp
    meshoptimizer/src/vertexcodec.cpp
    meshoptimizer/src/vertexfilter.cpp
    meshoptimizer/src/vfetchanalyzer.cpp
    meshoptimizer/src/vfetchoptimizer.cpp
    meshoptimizer/demo/objparser.h
    meshoptimizer/demo/objparser.cpp
    )

target_link_libraries(meshoptimizer PUBLIC Vulkan::Vulkan)

add_subdirectory(volk)


