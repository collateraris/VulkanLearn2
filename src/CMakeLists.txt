
# Add source to this project's executable.
add_executable(vulkan_guide
    main.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_mesh.h
    vk_mesh.cpp
    vk_textures.h
    vk_textures.cpp
    vk_scene.h
    vk_scene.cpp
    vk_ioutils.h
    vk_ioutils.cpp
    vk_assimp_loader.h
    vk_assimp_loader.cpp
    vk_material_system.h
    vk_material_system.cpp
    vk_descriptors.h
    vk_descriptors.cpp
    vk_shaders.h
    vk_shaders.cpp
    vk_camera.h
    vk_camera.cpp
    vk_resource_manager.h
    vk_resource_manager.cpp)


set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_compile_definitions(vulkan_guide PRIVATE VK_USE_PLATFORM_WIN32_KHR)

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image assimp spirv_reflect)

target_link_libraries(vulkan_guide Vulkan::Vulkan sdl2 volk)

add_dependencies(vulkan_guide Shaders)
