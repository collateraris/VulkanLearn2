
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
    vk_resource_manager.cpp
    imgui_menu.h
    vk_logger.h
    vk_logger.cpp
    vk_render_passes.h
    vk_render_passes.cpp
    vk_render_graph.h
    vk_render_graph.cpp
    vk_raytracer_builder.h
    vk_raytracer_builder.cpp
    vk_utils.h
    vk_utils.cpp
    vk_command_buffer.h
    vk_command_buffer.cpp
    vk_render_pass.h
    vk_render_pass.cpp
    vk_framebuffer.h
    vk_framebuffer.cpp
    vk_command_pool.h
    vk_command_pool.cpp
    vk_render_pipeline.h
    vk_render_pipeline.cpp
    graphic_pipeline/vk_fullscreen_graphics_pipeline.h
    graphic_pipeline/vk_fullscreen_graphics_pipeline.cpp
    graphic_pipeline/vbuffer_graphics_pipeline.h
    graphic_pipeline/vbuffer_graphics_pipeline.cpp
    graphic_pipeline/vk_vbuffer_shading_graphics_pipeline.h
    graphic_pipeline/vk_vbuffer_shading_graphics_pipeline.cpp
    graphic_pipeline/vk_gbuffer_graphics_pipeline.h
    graphic_pipeline/vk_gbuffer_graphics_pipeline.cpp
    graphic_pipeline/vk_gbuffer_shading_graphics_pipeline.h
    graphic_pipeline/vk_gbuffer_shading_graphics_pipeline.cpp
    graphic_pipeline/vk_gi_raytrace_graphics_pipeline.h
    graphic_pipeline/vk_gi_raytrace_graphics_pipeline.cpp    
    graphic_pipeline/vk_restir_init_plus_temporal_pass.h
    graphic_pipeline/vk_restir_init_plus_temporal_pass.cpp
    graphic_pipeline/vk_restir_spacial_reuse_pass.h
    graphic_pipeline/vk_restir_spacial_reuse_pass.cpp
    graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.h
    graphic_pipeline/vk_restir_update_reservoir_plus_shade_pass.cpp
    graphic_pipeline/vk_simple_accumulation_graphics_pipeline.h
    graphic_pipeline/vk_simple_accumulation_graphics_pipeline.cpp
    graphic_pipeline/vk_ibl_maps_generator_graphics_pipeline.h
    graphic_pipeline/vk_ibl_maps_generator_graphics_pipeline.cpp
    vk_light_manager.h
    vk_light_manager.cpp
    sys_config/ConfigManager.h
    sys_config/ConfigManager.cpp
    sys_config/Config.h
    sys_config/Config.cpp
    sys_config/XPath.h
    sys_config/XPath.cpp
    sys_config/tinyxml2.h
    sys_config/tinyxml2.cpp
    sys_config/vk_strings.h
    sl_wrapper/SLWrapper.h
    sl_wrapper/SLWrapper.cpp)


set_property(TARGET vulkan_guide PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:vulkan_guide>")

target_include_directories(vulkan_guide PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(vulkan_guide vkbootstrap vma glm tinyobjloader imgui stb_image assimp spirv_reflect)

target_link_libraries(vulkan_guide sdl2 volk volk_headers meshoptimizer plog taskflow streamline) 

add_dependencies(vulkan_guide Shaders)
