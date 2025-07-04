﻿
#include "vk_engine.h"
#include "vk_textures.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_utils.h>
#include <vk_initializers.h>

//bootstrap library
#include "VkBootstrap.h"

#include <iostream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <imgui_menu.h>

#include <vk_assimp_loader.h>

#include <sys_config/ConfigManager.h>
#include <sys_config/vk_strings.h>

#include <sl_wrapper/SLWrapper.h>
using namespace vk_utils;

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
void VK_CHECK(VkResult result_) {
	if (result_ != VK_SUCCESS)
	{
		assert(false);
		std::cout << "VkResult";
	}
}

ERenderMode VulkanEngine::get_mode()
{
	return vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetRenderMode();
}

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

	_windowExtent.width = vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetWindowWidth();
	_windowExtent.height = vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetWindowHeight();
	
	_window = SDL_CreateWindow(
		vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetTitle().c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);
	SceneConfig config = vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetCurrentScene();
	AsimpLoader::processScene(config, _scene, _resManager, config.model);

	_camera.init();

	_logger.init("vulkan.log");

	_rgraph.init(this);

	_depthReduceRenderPass.init(this);

	//load the core Vulkan structures
	init_vulkan();

	_shaderCache.init(_device);

	ResourceManager::init_samplers(this, _resManager);

	//create the swapchain
	init_swapchain();

	init_commands();

	init_sync_structures();

	immediate_submit([&](VkCommandBuffer cmd) {
		std::array<VkImageMemoryBarrier, 2> offscreenBarriers =
		{
			vkinit::image_barrier(_depthTex.image._image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
			vkinit::image_barrier(_swapchainTextures[0].image._image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, offscreenBarriers.size(), offscreenBarriers.data());

		});

	init_default_renderpass();

	init_framebuffers();


	init_descriptors();

	init_imgui();


	ResourceManager::load_images(this, _resManager.textureCache);

	ResourceManager::load_meshes(this, _resManager.meshList);

	init_scene();
	init_pipelines();

	_lightManager.init(this);
	if (config.lightConfig.bUseSun) {
		_lightManager.add_sun_light(std::move(config.lightConfig.sunDirection), std::move(config.lightConfig.sunColor));
	}
	if (config.lightConfig.bUseUniformGeneratePointLight)
	{
		_lightManager.generate_uniform_grid(_resManager.maxCube, _resManager.minCube, config.lightConfig.numUniformPointLightPerAxis);
	}
	if (config.lightConfig.bUseSun || config.lightConfig.bUseUniformGeneratePointLight || (_lightManager.get_lights().size() > 0))
	{
		_lightManager.create_cpu_host_visible_light_buffer();
	}

	_fluxGenerationGraphicsPipeline.init(this);
	immediate_submit([&](VkCommandBuffer cmd) {
		std::array<VkBufferMemoryBarrier, 1> barriers =
		{
			vkinit::buffer_barrier(_lightManager.get_light_buffer()._buffer, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT),
		};

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, barriers.size(), barriers.data(), 0, 0);

		_fluxGenerationGraphicsPipeline.draw(cmd, _lightManager.get_lights().size());
		});
	_lightManager.update_light_data_from_gpu();
	_lightManager.update_lights_alias_table();

	
	//_iblGenGraphicsPipeline.init(this, config.hdrCubemapPath);
	ResourceManager::init_rt_scene(this, _resManager);
	if (get_mode() == ERenderMode::ReSTIR_NRC)
	{
		ResourceManager::init_nrc_cache(this, _resManager);
	}
	ResourceManager::init_global_bindless_descriptor(this, _resManager);

#if VBUFFER_ON
	_visBufGenerateGraphicsPipeline.init(this);
#endif
#if GBUFFER_ON
	_gBufGenerateGraphicsPipeline.init(this);
#endif	

	if (get_mode() == ERenderMode::ReSTIR || get_mode() == ERenderMode::ReSTIR_NRC)
	{
		_giRtGraphicsPipeline.init_textures(this);
		_giRtGraphicsPipeline.init(this);
		_gBufShadingGraphicsPipeline.init(this, _giRtGraphicsPipeline.get_output());
	}


#if ReSTIR_PATHTRACER_ON
	_ptReSTIRGraphicsPipeline.init_textures(this);
	_ptReSTIRGraphicsPipeline.init(this);
	_gBufShadingGraphicsPipeline.init(this, _ptReSTIRGraphicsPipeline.get_output());
#endif

	if (get_mode() == ERenderMode::Pathtracer)
	{
		_ptReference.init(this);
		_accumulationGP.init(this, _ptReference.get_tex(ETextureResourceNames::PT_REFERENCE_OUTPUT));
		_gBufShadingGraphicsPipeline.init(this, _accumulationGP.get_output());
	}

	_camera = {};
	_camera.position = { 0.f,-6.f,-10.f };
	if (config.bUseCustomCam)
	{
		_camera.position = config.camPos;
		_camera.pitch = config.camPith;
		_camera.yaw = config.camYaw;
	}
	
	//everything went fine
	_isInitialized = true;
}
void VulkanEngine::cleanup()
{	
	if (_isInitialized) {

		//make sure the GPU has stopped doing its things
		for (auto& frame : _frames)
		{
			vkWaitForFences(_device, 1, &frame._renderFence, true, 1000000000);
		}

		_mainDeletionQueue.flush();

		_descriptorAllocator->cleanup();
		_descriptorLayoutCache->cleanup();

		_descriptorBindlessAllocator->cleanup();
		_descriptorBindlessLayoutCache->cleanup();

#if STREAMLINE_ON
		SLWrapper::Get().Shutdown();
#endif

		vkDestroyDevice(_device, nullptr);
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw()
{
	//wait until the GPU has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
	VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

	//request image from the swapchain, one second timeout
	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._presentSemaphore, nullptr, &swapchainImageIndex));

	//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
	get_current_frame()._mainCommandBuffer.reset();

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer.get_cmd();

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	get_current_frame()._mainCommandBuffer.record(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, [&]() 
	{
		{
			vkCmdResetQueryPool(cmd, get_current_frame().queryPool, 0, 128);
			vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, get_current_frame().queryPool, 0);

			//camera view
			glm::mat4 view = _camera.get_view_matrix();
			//camera projection
			glm::mat4 projection = _camera.get_projection_matrix();

			//fill a GPU camera data struct
			GPUCameraData camData;
			camData.proj = projection;
			camData.view = view;
			camData.viewproj = projection * view;
			camData.znear = _camera.nearDistance;
			camData.pyramidWidth = _depthReduceRenderPass.get_depthPyramidTex().extend.width;
			camData.pyramidHeight = _depthReduceRenderPass.get_depthPyramidTex().extend.height;

			PerFrameData frustumData;
			std::array<glm::vec4, 6> frustum = _camera.calcFrustumPlanes();
			for (int i = 0; i < frustum.size(); ++i)
				camData.frustumPlanes[i] = frustumData.frustumPlanes[i] = frustum[i];

			map_buffer(_allocator, get_current_frame().cameraBuffer._allocation, [&](void*& data) {
				memcpy(data, &camData, sizeof(GPUCameraData));
				});

#if VBUFFER_ON
			{
				VulkanVbufferGraphicsPipeline::SGlobalCamera globalCameraData;
				globalCameraData.viewProj = projection * view;
				_visBufGenerateGraphicsPipeline.copy_global_uniform_data(globalCameraData, get_current_frame_index());
			}
#endif
#if GBUFFER_ON
			{
				VulkanGbufferGenerateGraphicsPipeline::SGlobalCamera globalCameraData;
				globalCameraData.viewProj = projection * view;
				_gBufGenerateGraphicsPipeline.copy_global_uniform_data(globalCameraData, get_current_frame_index());
			}
#endif
			if (get_mode() == ERenderMode::ReSTIR || get_mode() == ERenderMode::ReSTIR_NRC)
			{
				_giRtGraphicsPipeline.try_reset_accumulation(_camera);
			}

			if (get_mode() == ERenderMode::Pathtracer)
			{
				_accumulationGP.try_reset_accumulation(_camera);
			}

		}

		//compute_pass(cmd); 
#if VBUFFER_ON
		_visBufGenerateGraphicsPipeline.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index());
#endif			
#if GBUFFER_ON
		_gBufGenerateGraphicsPipeline.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index());
		_gBufGenerateGraphicsPipeline.barrier_for_gbuffer_shading(&get_current_frame()._mainCommandBuffer);
#endif
#if ReSTIR_PATHTRACER_ON
	_ptReSTIRGraphicsPipeline.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index());
	_ptReSTIRGraphicsPipeline.barrier_for_frag_read(&get_current_frame()._mainCommandBuffer);
#endif
	if (get_mode() == ERenderMode::Pathtracer)
	{
		_ptReference.barrier_for_writing(&get_current_frame()._mainCommandBuffer);
		_ptReference.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index());
		_ptReference.barrier_for_reading(&get_current_frame()._mainCommandBuffer);
		_accumulationGP.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index(), ERenderMode::Pathtracer);
	}	
	if (get_mode() == ERenderMode::ReSTIR || get_mode() == ERenderMode::ReSTIR_NRC)
	{
		_giRtGraphicsPipeline.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index());
	}
		{
			//make a clear-color from frame number. This will flash with a 120*pi frame period.
			VkClearValue clearValue;
			clearValue.color = { { 0.0f, 0.0f, 0.f, 1.0f } };

			//clear depth at 1
			VkClearValue depthClear;
			depthClear.depthStencil.depth = 1.f;
			//start the main renderpass. 
			//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
			VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass(), _windowExtent, _framebuffers[swapchainImageIndex]);

			//connect clear values
			rpInfo.clearValueCount = 2;

			VkClearValue clearValues[] = { clearValue, depthClear };

			rpInfo.pClearValues = &clearValues[0];

			vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport;
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)_windowExtent.width;
			viewport.height = (float)_windowExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor;
			scissor.offset = { 0, 0 };
			scissor.extent = _windowExtent;

			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);
			vkCmdSetDepthBias(cmd, 0, 0, 0);
			
			if (get_mode() == ERenderMode::Pathtracer || get_mode() == ERenderMode::ReSTIR || get_mode() == ERenderMode::ReSTIR_NRC)
			{
				_gBufShadingGraphicsPipeline.draw(&get_current_frame()._mainCommandBuffer, get_current_frame_index());
			}			
		//draw_objects(cmd, _renderables.data(), _renderables.size());
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

			//finalize the render pass
			vkCmdEndRenderPass(cmd);
		}
#if GBUFFER_ON
		{
			_gBufGenerateGraphicsPipeline.barrier_for_gbuffer_generate(&get_current_frame()._mainCommandBuffer);
		}
#endif		
		//_depthReduceRenderPass.compute_pass(cmd, _frameNumber% FRAME_OVERLAP, { Resources{ &_depthTex} });

		vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, get_current_frame().queryPool, 1);
	});
	//prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &get_current_frame()._presentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &get_current_frame()._renderSemaphore;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, get_current_frame()._renderFence));

	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as it's necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;
	 
	presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	uint64_t queryResults[2];
#if VULKAN_DEBUG_ON 
	//VK_CHECK(vkGetQueryPoolResults(_device, get_current_frame().queryPool, 0, 2, sizeof(queryResults), queryResults, sizeof(queryResults[0]), VK_QUERY_RESULT_64_BIT));

	//double frameGpuBegin = double(queryResults[0]) * _physDevProp.limits.timestampPeriod * 1e-6;
	//double frameGpuEnd = double(queryResults[1]) * _physDevProp.limits.timestampPeriod * 1e-6;

	_stats.frameGpuAvg = 0;// _stats.frameGpuAvg * 0.95 + (frameGpuEnd - frameGpuBegin) * 0.05;
#endif	
	//increase the number of frames drawn
	_frameNumber++;
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	std::chrono::time_point<std::chrono::system_clock> start, end;

	start = std::chrono::system_clock::now();
	end = std::chrono::system_clock::now();

	std::chrono::time_point<std::chrono::high_resolution_clock> frameCpuStart;
	std::chrono::time_point<std::chrono::high_resolution_clock> frameCpuEnd;

	//main loop
	while (!bQuit)
	{
		_stats.triangleCount = 0;
		_stats.trianglesPerSec = 0;

		frameCpuStart = std::chrono::high_resolution_clock::now();
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;
			_camera.process_input_event(&e);
			

			ImGui_ImplSDL2_ProcessEvent(&e);
		}

		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed_seconds = end - start;
		float frametime = elapsed_seconds.count() * 1000.f;
		_camera.update_camera(frametime);
		_camera.update_jitter(_windowExtent.width, _windowExtent.height);

		start = std::chrono::system_clock::now();

		//imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(_window);

		ImGui::NewFrame();

		//imgui commands
		ImguiAppLog::ShowVkMenu(*this);

		ImGui::Render();

		draw();

		frameCpuEnd = std::chrono::high_resolution_clock::now();

		{
			std::chrono::duration<double> elapsed_seconds = frameCpuEnd - frameCpuStart;
			_stats.frameCpuAvg = _stats.frameCpuAvg * 0.95 + elapsed_seconds / std::chrono::milliseconds(1) * 0.05;
		}
	}
}

AllocatedSampler* VulkanEngine::get_engine_sampler(ESamplerType samplerNameId)
{
	return _resManager.create_engine_sampler(samplerNameId);
}

Texture* VulkanEngine::get_engine_texture(ETextureResourceNames texNameId)
{
	return _resManager.get_engine_texture(texNameId);
}

AllocateDescriptor* VulkanEngine::get_engine_descriptor(EDescriptorResourceNames descrNameId)
{
	return _resManager.get_engine_descriptor(descrNameId);
}

void VulkanEngine::init_vulkan()
{
#if STREAMLINE_ON
	SLWrapper::Get().Initialize_preDevice(this);
#endif
	VK_CHECK(volkInitialize());

	vkb::InstanceBuilder builder;
	//make the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("My Vulkan pet project")
		.require_api_version(1, 4, 3)
#if VULKAN_DEBUG_ON
		.request_validation_layers(true)
		.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)
		.enable_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
		.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
		.set_debug_callback(&vk_logger_debug_callback)
		.add_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
#endif
		.build();

	vkb::Instance vkb_inst = inst_ret.value();

	//store the instance
	_instance = vkb_inst.instance;
	volkLoadInstance(_instance);
	//store the debug messenger
	_debug_messenger = vkb_inst.debug_messenger;

	// get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	VkPhysicalDeviceFeatures required_features = {};
	required_features.multiDrawIndirect = 1;
	required_features.robustBufferAccess = 1;
	required_features.drawIndirectFirstInstance = 1;
	required_features.shaderUniformBufferArrayDynamicIndexing = 1;
	required_features.shaderSampledImageArrayDynamicIndexing = 1;
	required_features.shaderStorageBufferArrayDynamicIndexing = 1;
	required_features.shaderStorageImageArrayDynamicIndexing = 1;
	required_features.geometryShader = 1;

	std::vector<const char*> extensions = {
		VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_NV_MESH_SHADER_EXTENSION_NAME,
		VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // Required by ray tracing pipeline
		VK_EXT_SHADER_REPLICATED_COMPOSITES_EXTENSION_NAME,

#if VULKAN_RAYTRACE_DEBUG_ON		
		VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME,
#endif	
		VK_NV_COOPERATIVE_VECTOR_EXTENSION_NAME,
	};

	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 4)
		.set_surface(_surface)
		.add_required_extensions(extensions)
		.set_required_features(required_features)
		.select()
		.value();

	//create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
	shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	shader_draw_parameters_features.pNext = nullptr;
	shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;

	VkPhysicalDeviceMeshShaderFeaturesNV featuresMesh = { };
	featuresMesh.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
	featuresMesh.pNext = nullptr;
	featuresMesh.meshShader = true;
	featuresMesh.taskShader = true;

	VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features = {};
	buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	buffer_device_address_features.pNext = nullptr;
	buffer_device_address_features.bufferDeviceAddress = true;

	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
	descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptor_indexing_features.pNext = nullptr;
	descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = true;
	descriptor_indexing_features.descriptorBindingSampledImageUpdateAfterBind = true;
	descriptor_indexing_features.shaderUniformBufferArrayNonUniformIndexing = true;
	descriptor_indexing_features.descriptorBindingUniformBufferUpdateAfterBind = true;
	descriptor_indexing_features.shaderStorageBufferArrayNonUniformIndexing = true;
	descriptor_indexing_features.descriptorBindingStorageBufferUpdateAfterBind = true;
	descriptor_indexing_features.descriptorBindingVariableDescriptorCount = true;
	descriptor_indexing_features.descriptorBindingPartiallyBound = true;
	descriptor_indexing_features.runtimeDescriptorArray = true;

	VkPhysicalDeviceVulkan13Features phys_dev_13_features{};
	phys_dev_13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	phys_dev_13_features.maintenance4 = true;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {};
	acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	acceleration_structure_features.pNext = nullptr;
	acceleration_structure_features.accelerationStructure = true;
	acceleration_structure_features.descriptorBindingAccelerationStructureUpdateAfterBind = true;


	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_features = {};
	rt_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rt_features.pNext = nullptr;
	rt_features.rayTracingPipeline = true;

	VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {};
	ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	ray_query_features.rayQuery = true;
	ray_query_features.pNext = nullptr;

	//VkPhysicalDeviceSynchronization2Features synchronized2_features = {};
	//synchronized2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	//synchronized2_features.pNext = nullptr;
	//synchronized2_features.synchronization2 = true;

	VkPhysicalDeviceRayTracingValidationFeaturesNV rtValidationFeatures = {};
	rtValidationFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV;
	rtValidationFeatures.pNext = nullptr;
	rtValidationFeatures.rayTracingValidation = true;

	VkPhysicalDeviceCooperativeVectorPropertiesNV coopVecFeatures = {};
	coopVecFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_PROPERTIES_NV;
	coopVecFeatures.pNext = nullptr;
	coopVecFeatures.cooperativeVectorSupportedStages = VK_SHADER_STAGE_COMPUTE_BIT;
	coopVecFeatures.cooperativeVectorTrainingFloat16Accumulation = true;
	coopVecFeatures.cooperativeVectorTrainingFloat32Accumulation = true;

	VkPhysicalDeviceCooperativeVectorFeaturesNV coopVecFeatures2 = {};
	coopVecFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_FEATURES_NV;
	coopVecFeatures2.pNext = nullptr;
	coopVecFeatures2.cooperativeVector = true;

	VkPhysicalDeviceVulkan12Features vulkan12Features = {};
	vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan12Features.pNext = nullptr;
	vulkan12Features.shaderFloat16 = true;

	VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT shaderReplicatedFeatures = {};
	shaderReplicatedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT;
	shaderReplicatedFeatures.pNext = nullptr;
	shaderReplicatedFeatures.shaderReplicatedComposites = true;

	VkPhysicalDevice16BitStorageFeatures  _16bitstorageFeatures = {};
	_16bitstorageFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT;
	_16bitstorageFeatures.pNext = nullptr;
	_16bitstorageFeatures.storageBuffer16BitAccess = true;



	vkb::Device vkbDevice = deviceBuilder.add_pNext(&shader_draw_parameters_features)
		.add_pNext(&featuresMesh)
		.add_pNext(&buffer_device_address_features)
		.add_pNext(&descriptor_indexing_features)
		.add_pNext(&phys_dev_13_features)
		.add_pNext(&acceleration_structure_features)
		.add_pNext(&rt_features)
		.add_pNext(&ray_query_features)
		.add_pNext(&coopVecFeatures)
		.add_pNext(&coopVecFeatures2)
		.add_pNext(&vulkan12Features)
		.add_pNext(&shaderReplicatedFeatures)
		.add_pNext(&_16bitstorageFeatures)
		//.add_pNext(&synchronized2_features)
	#if VULKAN_RAYTRACE_DEBUG_ON	
		.add_pNext(&rtValidationFeatures)
	#endif	
		.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenPhysicalDeviceGPU = physicalDevice.physical_device;

	vkGetPhysicalDeviceProperties(_chosenPhysicalDeviceGPU, &_physDevProp);

	// use vkbootstrap to get a Graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	
	//initialize the memory allocator

	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;


	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenPhysicalDeviceGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;
	allocatorInfo.vulkanApiVersion = VK_MAKE_VERSION(1, 4, 3);
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_gpuProperties = vkbDevice.physical_device.properties;

	{
		VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		prop2.pNext = &_rtProperties;
		vkGetPhysicalDeviceProperties2(_chosenPhysicalDeviceGPU, &prop2);
	}	

	std::cout << "The GPU has a minimum buffer alignment of " << _gpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{_chosenPhysicalDeviceGPU, _device, _surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use vsync present mode
		.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT| VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;

	std::vector<VkImage> swapchainImages = {};
	std::vector<VkImageView> swapchainImageViews = {};

	swapchainImages = vkbSwapchain.get_images().value();
	swapchainImageViews = vkbSwapchain.get_image_views().value();

	for (uint32_t i = 0; i < swapchainImages.size(); i++)
	{
		Texture swapchainTex;
		swapchainTex.image._image = swapchainImages[i];
		swapchainTex.imageView = swapchainImageViews[i];
		swapchainTex.extend.width = _windowExtent.width;
		swapchainTex.extend.height = _windowExtent.height;
		swapchainTex.createInfo.format = vkbSwapchain.image_format;
		swapchainTex.bIsSwapChainImage = true;
		swapchainTex.createInfo.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		swapchainTex.createInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		_swapchainTextures.push_back(swapchainTex);
	}

	_swapchainImageFormat = vkbSwapchain.image_format;

	_mainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
	});

	//depth image size will match the window
	VkExtent3D depthImageExtent = {
		_windowExtent.width,
		_windowExtent.height,
		1
	};

	//hardcoding the depth format to 32 bit float
	_depthFormat = VK_FORMAT_D32_SFLOAT;

	VulkanTextureBuilder texBuilder;
	texBuilder.init(this);
	_depthTex = texBuilder.start()
		.make_img_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, depthImageExtent)
		.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		.make_view_info(_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
		.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
		.create_texture();
}

void VulkanEngine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {
		_frames[i]._commandPool.init(this, commandPoolInfo);

		_frames[i]._mainCommandBuffer.init(_frames[i]._commandPool.request_command_buffer());
	}

	VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);
	
	_uploadContext._commandPool.init(this, uploadCommandPoolInfo);
	_uploadContext._commandBuffer.init(_uploadContext._commandPool.request_command_buffer());

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_frames[i].queryPool = createQueryPool(128);
	}
}

void VulkanEngine::init_default_renderpass()
{
	_renderPassManager.init(this);

	RenderPassInfo default_rp;
	default_rp.op_flags = RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
	default_rp.clear_attachments = 1 << 0;
	default_rp.store_attachments = 1 << 0;
	default_rp.num_color_attachments = 1;
	default_rp.color_attachments[0] = &_swapchainTextures[0];
	default_rp.depth_stencil = &_depthTex;

	RenderPassInfo::Subpass subpass = {};
	subpass.num_color_attachments = 1;
	subpass.depth_stencil_mode = RenderPassInfo::DepthStencil::ReadWrite;
	subpass.color_attachments[0] = 0;
	subpass.resolve_attachments[0] = 1;

	default_rp.num_subpasses = 1;
	default_rp.subpasses = &subpass;

	_renderPassManager.get_render_pass(ERenderPassType::Default)->init(this, default_rp);
}

void VulkanEngine::init_framebuffers()
{
	_framebufferManager.init(this);
	//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = _renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass();
	fb_info.attachmentCount = 1;
	fb_info.width = _windowExtent.width;
	fb_info.height = _windowExtent.height;
	fb_info.layers = 1;

	//grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = _swapchainTextures.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	//create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++) {

		VkImageView attachments[2];
		attachments[0] = _swapchainTextures[i].imageView;
		attachments[1] = _depthTex.imageView;

		fb_info.pAttachments = attachments;
		fb_info.attachmentCount = 2;
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainTextures[i].imageView, nullptr);
			});
	}
}

void VulkanEngine::init_sync_structures()
{
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

	VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

	for (int i = 0; i < FRAME_OVERLAP; i++) {

		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));

		//enqueue the destruction of the fence
		_mainDeletionQueue.push_function([=]() {
			vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
			});


		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._presentSemaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));

		//enqueue the destruction of semaphores
		_mainDeletionQueue.push_function([=]() {
			vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
			});
	}

	VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();

	VK_CHECK(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext._uploadFence));
	_mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
		});
}

void VulkanEngine::init_pipelines() {

	_renderPipelineManager.init(this, &_renderPassManager, &_shaderCache);

#if MESHSHADER_ON
	_renderPipelineManager.init_render_pipeline(this, EPipelineType::Bindless_TaskMeshIndirectForward,
		[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
			ShaderEffect defaultEffect;
			defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.task.spv")), VK_SHADER_STAGE_TASK_BIT_NV);
			defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.mesh.spv")), VK_SHADER_STAGE_MESH_BIT_NV);
			defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);

			defaultEffect.reflect_layout(_device, nullptr, 0);
			//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
			GraphicPipelineBuilder pipelineBuilder;

			pipelineBuilder.setShaders(&defaultEffect);
			VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
			std::vector<VkDescriptorSetLayout> setLayout = { _bindlessSetLayout, _globalSetLayout };
			mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
			mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

			VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &pipelineBuilder._pipelineLayout));
			VkPipelineLayout meshPipLayout = pipelineBuilder._pipelineLayout;

			//vertex input controls how to read vertices from vertex buffers. We arent using it yet
			pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

			//input assembly is the configuration for drawing triangle lists, strips, or individual points.
			//we are just going to draw triangle list
			pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

			//build viewport and scissor from the swapchain extents
			pipelineBuilder._viewport.x = 0.0f;
			pipelineBuilder._viewport.y = 0.0f;
			pipelineBuilder._viewport.width = _windowExtent.width;
			pipelineBuilder._viewport.height = _windowExtent.height;
			pipelineBuilder._viewport.minDepth = 0.0f;
			pipelineBuilder._viewport.maxDepth = 1.0f;

			pipelineBuilder._scissor.offset = { 0, 0 };
			pipelineBuilder._scissor.extent = _windowExtent;

			//configure the rasterizer to draw filled triangles
			pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

			//we dont use multisampling, so just run the default one
			pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

			//a single blend attachment with no blending and writing to RGBA
			pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());

			//default depthtesting
			pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
		
			VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(_device,
				_renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass());

			pipeline = meshPipeline;
			pipelineLayout = meshPipLayout;
		});
#endif

#if MESHSHADER_ON
	_depthReduceRenderPass.init_pipelines();
#endif	
}

std::string VulkanEngine::asset_path(std::string_view path)
{
	return "../../assets_export/" + std::string(path);
}

std::string VulkanEngine::shader_path(std::string_view path)
{
	return vk_utils::SHADERS_PATH + std::string(path);
}
std::string VulkanEngine::shader_slang_path(std::string_view path)
{
	return vk_utils::SHADERS_SLANG_PATH + std::string(path);
}

VkQueryPool VulkanEngine::createQueryPool(uint32_t queryCount)
{
	VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	createInfo.queryCount = queryCount;

	VkQueryPool queryPool = 0;
	VK_CHECK(vkCreateQueryPool(_device, &createInfo, 0, &queryPool));

	return queryPool;
}

uint64_t VulkanEngine::padSizeToMinUniformBufferOffsetAlignment(uint64_t originalSize)
{
	uint64_t minAlignment = _physDevProp.limits.minUniformBufferOffsetAlignment;
	return vkutil::padSizeToAlignment(originalSize, minAlignment);
}

uint64_t VulkanEngine::padSizeToMinStorageBufferOffsetAlignment(uint64_t originalSize)
{
	uint64_t minAlignment = _physDevProp.limits.minStorageBufferOffsetAlignment;
	return vkutil::padSizeToAlignment(originalSize, minAlignment);
}

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
}

int VulkanEngine::get_current_frame_index() const
{
	return _frameNumber % FRAME_OVERLAP;
}

size_t VulkanEngine::pad_uniform_buffer_size(size_t originalSize)
{
	// Calculate required alignment based on minimum device offset alignment
	size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

AllocatedBuffer VulkanEngine::create_buffer_n_copy_data(size_t allocSize, void* copyData, VkBufferUsageFlags usage)
{
	AllocatedBuffer resBuffer;
	if (allocSize)
	{
		//allocate staging buffer
		AllocatedBuffer stagingBuffer = create_staging_buffer(allocSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		//copy vertex data

		map_buffer(_allocator, stagingBuffer._allocation, [&](void*& data) {
			memcpy(data, copyData, allocSize);
			});

		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		{
			resBuffer = create_gpuonly_buffer_with_device_address(allocSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		}
		else
		{
			resBuffer = create_gpuonly_buffer(allocSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		}

		immediate_submit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = VkDeviceSize(allocSize);
			vkCmdCopyBuffer(cmd, stagingBuffer._buffer, resBuffer._buffer, 1, &copy);
			});

		//add the destruction of mesh buffer to the deletion queue
		_mainDeletionQueue.push_function([&]() {
			destroy_buffer(_allocator, resBuffer);
			});

		destroy_buffer(_allocator, stagingBuffer);
	}
	return resBuffer;
}

AllocatedBuffer VulkanEngine::create_gpuonly_buffer(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_buffer(allocSize, usage, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
}

AllocatedBuffer VulkanEngine::create_gpuonly_buffer_with_device_address(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_buffer(allocSize, usage, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT);
}

AllocatedBuffer VulkanEngine::create_staging_buffer(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_buffer(allocSize, usage, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

AllocatedBuffer VulkanEngine::create_cpu_to_gpu_buffer(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_staging_buffer(allocSize, usage);
}

void VulkanEngine::destroy_buffer(VmaAllocator& allocator, AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(allocator, buffer._buffer, buffer._allocation);
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags)
{
	//allocate vertex buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;

	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;


	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	vmaallocInfo.flags = flags;

	AllocatedBuffer newBuffer;

	//allocate the buffer
	VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
		&newBuffer._buffer,
		&newBuffer._allocation,
		nullptr));

	newBuffer._size = newBuffer._allocation->GetSize();

	return newBuffer;
}

void VulkanEngine::map_buffer(VmaAllocator& allocator, VmaAllocation& allocation, std::function<void(void*& data)> func)
{
	void* data;
	vmaMapMemory(allocator, allocation, &data);

	func(data);

	vmaUnmapMemory(allocator, allocation);
}

void VulkanEngine::write_buffer(VmaAllocator& allocator, VmaAllocation& allocation, const void* srcData, size_t dataSize)
{
	map_buffer(allocator, allocation, [&](void*& data) {
		memcpy(data, srcData, dataSize);
		});
}

void VulkanEngine::create_image(const VkImageCreateInfo& _img_info, const VmaAllocationCreateInfo& _img_allocinfo, VkImage& img, VmaAllocation& img_alloc, VmaAllocationInfo* vma_allocinfo)
{
	vmaCreateImage(_allocator, &_img_info, &_img_allocinfo, &img, &img_alloc, vma_allocinfo);

	_mainDeletionQueue.push_function([=]() {
		vmaDestroyImage(_allocator, img, img_alloc);
		});
}

void VulkanEngine::create_image_view(const VkImageViewCreateInfo& _view_info, VkImageView& image_view)
{
	VK_CHECK(vkCreateImageView(_device, &_view_info, nullptr, &image_view));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyImageView(_device, image_view, nullptr);
		});
}

void VulkanEngine::create_render_pass(const VkRenderPassCreateInfo& info, VkRenderPass& renderPass)
{
	VK_CHECK(vkCreateRenderPass(_device, &info, nullptr, &renderPass));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyRenderPass(_device, renderPass, nullptr);
		});
}

VkDeviceAddress VulkanEngine::get_buffer_device_address(VkBuffer buffer)
{
	if (buffer == VK_NULL_HANDLE)
		return 0ULL;

	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = buffer;
	return vkGetBufferDeviceAddress(_device, &info);
}

void VulkanEngine::init_descriptors()
{
	//create a descriptor pool that will hold 10 uniform buffers and 10 dynamic uniform buffers
	_descriptorAllocator = std::make_unique<vkutil::DescriptorAllocator>();
	_descriptorAllocator->init(_device);

	_descriptorLayoutCache = std::make_unique<vkutil::DescriptorLayoutCache>();
	_descriptorLayoutCache->init(_device);

	_descriptorBindlessAllocator = std::make_unique<vkutil::DescriptorAllocator>();
	_descriptorBindlessAllocator->init(_device, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);

	_descriptorBindlessLayoutCache = std::make_unique<vkutil::DescriptorLayoutCache>();
	_descriptorBindlessLayoutCache->init(_device);

	//another set, one that holds a single texture
	VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

	VkDescriptorSetLayoutCreateInfo set1info = {};
	set1info.bindingCount = 1;
	set1info.flags = 0;
	set1info.pNext = nullptr;
	set1info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set1info.pBindings = &textureBind;

	_singleTextureSetLayout = _descriptorLayoutCache->create_descriptor_layout(&set1info);

	VkDescriptorSetLayoutBinding vertexBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, 0);
	VkDescriptorSetLayoutBinding meshletBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, 1);
	VkDescriptorSetLayoutBinding meshletdataBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, 2);

	std::vector<VkDescriptorSetLayoutBinding> meshletsBinding = {vertexBind, meshletBind, meshletdataBind };

	VkDescriptorSetLayoutCreateInfo set2info = {};
	set2info.bindingCount = meshletsBinding.size();
	set2info.flags = 0;
	set2info.pNext = nullptr;
	set2info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set2info.pBindings = meshletsBinding.data();

	_meshletsSetLayout = _descriptorLayoutCache->create_descriptor_layout(&set2info);

	const size_t sceneParamBufferSize = FRAME_OVERLAP * pad_uniform_buffer_size(sizeof(GPUSceneData));

	_sceneParameterBuffer = create_cpu_to_gpu_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	// add descriptor set layout to deletion queues
	_mainDeletionQueue.push_function([&]() {
		vmaDestroyBuffer(_allocator, _sceneParameterBuffer._buffer, _sceneParameterBuffer._allocation);
		vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);

		// add buffers to deletion queues
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			vmaDestroyBuffer(_allocator, _frames[i].cameraBuffer._buffer, _frames[i].cameraBuffer._allocation);
		}

		vmaDestroyBuffer(_allocator, _objectBuffer._buffer, _objectBuffer._allocation);
	});

#if MESHSHADER_ON
	_indirectBuffer = create_cpu_to_gpu_buffer(MAX_COMMANDS * sizeof(VkDrawMeshTasksIndirectCommandNV), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
#endif
	_objectBuffer = create_cpu_to_gpu_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	_depthReduceRenderPass.create_depth_pyramid(_windowExtent.width, _windowExtent.height);

	VkDescriptorImageInfo depthDescInfo = {};
	depthDescInfo.imageView = _depthTex.imageView;
	_depthReduceRenderPass.init_descriptors({ DescriptorInfo{ {}, depthDescInfo} });

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_frames[i].cameraBuffer = create_cpu_to_gpu_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = _objectBuffer._buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

		VkDescriptorBufferInfo cameraInfo;
		cameraInfo.buffer = _frames[i].cameraBuffer._buffer;
		cameraInfo.offset = 0;
		cameraInfo.range = sizeof(GPUCameraData);
#if MESHSHADER_ON

		VkDescriptorImageInfo depthPyramidImageBufferInfo;
		depthPyramidImageBufferInfo.sampler = _depthReduceRenderPass.get_depthSampl();
		depthPyramidImageBufferInfo.imageView = _depthReduceRenderPass.get_depthPyramidTex().imageView;
		depthPyramidImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_buffer(0, &cameraInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV)
			.bind_image(1, &depthPyramidImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV)
			.bind_buffer(2, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV)
			.build(_frames[i].globalDescriptor, _globalSetLayout);
#endif

#if MESHSHADER_ON

		VkDescriptorBufferInfo indirectBufferInfo;
		indirectBufferInfo.buffer = _indirectBuffer._buffer;
		indirectBufferInfo.offset = 0;
		indirectBufferInfo.range = sizeof(VkDrawMeshTasksIndirectCommandNV) * MAX_OBJECTS;

		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(1, &indirectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(2, &cameraInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_image(3, &depthPyramidImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(_frames[i].objectDescriptor, _objectSetLayout);
#endif
	}

}

void VulkanEngine::load_materials(VkPipeline pipeline, VkPipelineLayout layout)
{
	for (const auto& matDesc : _resManager.matDescList)
	{
		create_material(pipeline, layout, matDesc->matName);
	}
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VkCommandBuffer cmd = _uploadContext._commandBuffer.get_cmd();

	//begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//execute the function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = vkinit::submit_info(&cmd);

	//submit command buffer to the queue and execute it.
	// _uploadFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext._uploadFence));

	vkWaitForFences(_device, 1, &_uploadContext._uploadFence, true, 9999999999);
	vkResetFences(_device, 1, &_uploadContext._uploadFence);

	// reset the command buffers inside the command pool
	_uploadContext._commandPool.reset();
}

void VulkanEngine::immediate_submit2(std::function<void(VulkanCommandBuffer& cmd)>&& function)
{
	VkCommandBuffer cmd = _uploadContext._commandBuffer.get_cmd();

	//begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//execute the function
	function(_uploadContext._commandBuffer);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo submit = vkinit::submit_info(&cmd);

	//submit command buffer to the queue and execute it.
	// _uploadFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext._uploadFence));

	vkWaitForFences(_device, 1, &_uploadContext._uploadFence, true, 9999999999);
	vkResetFences(_device, 1, &_uploadContext._uploadFence);

	// reset the command buffers inside the command pool
	_uploadContext._commandPool.reset();
}

Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	Material mat;
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
	_materials[name] = mat;
	return &_materials[name];
}

Material* VulkanEngine::get_material(const std::string& name)
{
	//search for the object, and return nullpointer if not found
	auto it = _materials.find(name);
	if (it == _materials.end()) {
		return nullptr;
	}
	else {
		return &(*it).second;
	}
}

void VulkanEngine::compute_pass(VkCommandBuffer cmd)
{
#if MESHSHADER_ON
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _renderPipelineManager.get_pipeline(EPipelineType::ComputePrepassForTaskMeshIndirect));

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _renderPipelineManager.get_pipelineLayout(EPipelineType::ComputePrepassForTaskMeshIndirect), 0, 1, &get_current_frame().objectDescriptor, 0, nullptr);

	vkCmdDispatch(cmd, uint32_t((_renderables.size() + 31) / 32), 1, 1);

	std::array<VkBufferMemoryBarrier, 1> cullBarriers =
	{
		vkinit::buffer_barrier(_indirectBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
	};

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, 0, cullBarriers.size(), cullBarriers.data(), 0, 0);
#endif
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first, int count)
{
	if (count <= 0)
		return;

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;

	size_t frameIndex = _frameNumber % FRAME_OVERLAP;
#if MESHSHADER_ON
	//only bind the pipeline if it doesn't match with the already bound one
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _renderPipelineManager.get_pipeline(EPipelineType::Bindless_TaskMeshIndirectForward));

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _renderPipelineManager.get_pipelineLayout(EPipelineType::Bindless_TaskMeshIndirectForward), 0, 1, &_bindlessSet, 0, nullptr);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _renderPipelineManager.get_pipelineLayout(EPipelineType::Bindless_TaskMeshIndirectForward), 1, 1, &get_current_frame().globalDescriptor, 0, nullptr);

	VkDeviceSize indirect_offset = 0 * sizeof(VkDrawMeshTasksIndirectCommandNV);
	uint32_t draw_stride = sizeof(VkDrawMeshTasksIndirectCommandNV);
	vkCmdDrawMeshTasksIndirectNV(cmd, _indirectBuffer._buffer, indirect_offset, count, draw_stride);
#endif
}


void VulkanEngine::init_scene()
{
	ResourceManager::init_scene(this, _resManager, _scene);

#if MESHSHADER_ON
	map_buffer(_allocator, _objectBuffer._allocation, [&](void*& data) {
		GPUObjectData* objectSSBO = (GPUObjectData*)data;

		for (int i = 0; i < _renderables.size(); i++)
		{
			RenderObject& object = _renderables[i];
			objectSSBO[i].modelMatrix = object.transformMatrix;
			glm::vec3 center = glm::vec3(object.transformMatrix * glm::vec4(object.mesh->_center, 1.));
			objectSSBO[i].center_radius = glm::vec4(center, object.mesh->_radius);
			objectSSBO[i].meshIndex = object.meshIndex;
			objectSSBO[i].diffuseTexIndex = _resManager.matDescList[object.matDescIndex]->diffuseTextureIndex;
#if MESHSHADER_ON
			objectSSBO[i].meshletCount = object.mesh->_meshlets.size();
#endif
		}
		});
#endif

#if MESHSHADER_ON
	init_bindless_scene();
#endif
}

void VulkanEngine::init_bindless_scene()
{
#if MESHSHADER_ON
	const uint32_t meshletsVerticesBinding = 0;
	const uint32_t meshletsBinding = 1;
	const uint32_t meshletsDataBinding = 2;
	const uint32_t textureBinding = 3;

	//BIND MESHDATA
	std::vector<VkDescriptorBufferInfo> vertexBufferInfoList{};
	vertexBufferInfoList.resize(_resManager.meshList.size());

	std::vector<VkDescriptorBufferInfo> meshletBufferInfoList{};
	meshletBufferInfoList.resize(_resManager.meshList.size());

	std::vector<VkDescriptorBufferInfo> meshletdataBufferInfoList{};
	meshletdataBufferInfoList.resize(_resManager.meshList.size());
	
	for (uint32_t meshArrayIndex = 0; meshArrayIndex < _resManager.meshList.size(); meshArrayIndex++)
	{
		Mesh* mesh = _resManager.meshList[meshArrayIndex].get();

		VkDescriptorBufferInfo& vertexBufferInfo = vertexBufferInfoList[meshArrayIndex];
		vertexBufferInfo.buffer = mesh->_vertexBuffer._buffer;
		vertexBufferInfo.offset = 0;
		vertexBufferInfo.range = padSizeToMinStorageBufferOffsetAlignment(mesh->_vertexBuffer._allocation->GetSize()); 

		VkDescriptorBufferInfo&  meshletBufferInfo = meshletBufferInfoList[meshArrayIndex];
		meshletBufferInfo.buffer = mesh->_meshletsBuffer._buffer;
		meshletBufferInfo.offset = 0;
		meshletBufferInfo.range = padSizeToMinStorageBufferOffsetAlignment(mesh->_meshletsBuffer._allocation->GetSize());

		VkDescriptorBufferInfo& meshletdataBufferInfo = meshletdataBufferInfoList[meshArrayIndex];
		meshletdataBufferInfo.buffer = mesh->_meshletdataBuffer._buffer;
		meshletdataBufferInfo.offset = 0;
		meshletdataBufferInfo.range = padSizeToMinStorageBufferOffsetAlignment(mesh->_meshletdataBuffer._allocation->GetSize());
	}

	//BIND SAMPLERS
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VkSampler blockySampler;
	vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler);

	std::vector<VkDescriptorImageInfo> imageInfoList{};
	imageInfoList.resize(_resManager.textureList.size());

	for (uint32_t textureArrayIndex = 0; textureArrayIndex <  _resManager.textureList.size(); textureArrayIndex++)
	{
		VkDescriptorImageInfo& imageBufferInfo = imageInfoList[textureArrayIndex];
		imageBufferInfo.sampler = blockySampler;
		imageBufferInfo.imageView = _resManager.textureList[textureArrayIndex]->imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	vkutil::DescriptorBuilder::begin(_descriptorBindlessLayoutCache.get(), _descriptorBindlessAllocator.get())
		.bind_buffer(meshletsVerticesBinding, vertexBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, vertexBufferInfoList.size())
		.bind_buffer(meshletsBinding, meshletBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV, meshletBufferInfoList.size())
		.bind_buffer(meshletsDataBinding, meshletdataBufferInfoList.data(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV, meshletdataBufferInfoList.size())
		.bind_image(textureBinding, imageInfoList.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, imageInfoList.size())
		.build_bindless(_bindlessSet, _bindlessSetLayout);
#endif
}

void VulkanEngine::init_imgui()
{
	//1: create descriptor pool for IMGUI
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));


	// 2: initialize imgui library

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(_window);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _chosenPhysicalDeviceGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
		return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
		}, &_instance);

	ImGui_ImplVulkan_Init(&init_info, _renderPassManager.get_render_pass(ERenderPassType::Default)->get_render_pass());

	//execute a gpu command to upload imgui font textures
	immediate_submit([&](VkCommandBuffer cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	//add the destroy the imgui created structures
	_mainDeletionQueue.push_function([=]() {

		vkDestroyDescriptorPool(_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}



