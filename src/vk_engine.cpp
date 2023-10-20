
#include "vk_engine.h"
#include "vk_textures.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

//bootstrap library
#include "VkBootstrap.h"

#include <iostream>
#include <fstream>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <imgui_menu.h>

#include <vk_assimp_loader.h>

//we want to immediately abort when there is an error. In normal engines this would give an error message to the user, or perform a dump of state.
using namespace std;
void VK_CHECK(VkResult result_) {
	if (result_ != VK_SUCCESS)
		std::cout << "VkResult";
}

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	_window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_windowExtent.width,
		_windowExtent.height,
		window_flags
	);
	SceneConfig config;
	config.fileName = "../../assets/lost_empire.obj";
	//config.fileName = "../../assets/monkey_smooth.obj";
	config.scaleFactor = 0.9;
	AsimpLoader::processScene(config, _scene, _resManager);

	_logger.init("vulkan.log");

	_depthReduceRenderPass.init(this);

	//load the core Vulkan structures
	init_vulkan();

	_shaderCache.init(_device);

	//create the swapchain
	init_swapchain();

	init_commands();

	init_default_renderpass();

	init_framebuffers();

	init_sync_structures();

	init_descriptors();

	init_pipelines();

	init_imgui();

	load_images();

	load_meshes();

	init_scene();

	_camera = {};
	_camera.position = { 0.f,-6.f,-10.f };
	
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
	VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;

	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	{
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

		map_buffer(_allocator, get_current_frame().perframeDataBuffer._allocation, [&](void*& data) {
			memcpy(data, &frustumData, sizeof(PerFrameData));
			});
	}

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	vkCmdResetQueryPool(cmd, get_current_frame().queryPool, 0, 128);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, get_current_frame().queryPool, 0);

	compute_pass(cmd);

	//make a clear-color from frame number. This will flash with a 120*pi frame period.
	VkClearValue clearValue;
	float flash = abs(sin(_frameNumber / 120.f));
	clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

	//clear depth at 1
	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

	//start the main renderpass. 
	//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
	VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent, _framebuffers[swapchainImageIndex]);

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

	draw_objects(cmd, _renderables.data(), _renderables.size());

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	//finalize the render pass
	vkCmdEndRenderPass(cmd);

	_depthReduceRenderPass.compute_pass(cmd, _frameNumber% FRAME_OVERLAP, { Resources{ &_depthTex} });

	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, get_current_frame().queryPool, 1);
	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

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

	VK_CHECK(vkGetQueryPoolResults(_device, get_current_frame().queryPool, 0, 2, sizeof(queryResults), queryResults, sizeof(queryResults[0]), VK_QUERY_RESULT_64_BIT));

	double frameGpuBegin = double(queryResults[0]) * physDevProp.limits.timestampPeriod * 1e-6;
	double frameGpuEnd = double(queryResults[1]) * physDevProp.limits.timestampPeriod * 1e-6;

	_stats.frameGpuAvg = _stats.frameGpuAvg * 0.95 + (frameGpuEnd - frameGpuBegin) * 0.05;
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
		}

		end = std::chrono::system_clock::now();
		std::chrono::duration<float> elapsed_seconds = end - start;
		float frametime = elapsed_seconds.count() * 1000.f;
		_camera.update_camera(frametime);

		start = std::chrono::system_clock::now();

		//imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame(_window);

		ImGui::NewFrame();

		//imgui commands
		ImguiAppLog::ShowFPSLog(_stats);

		ImGui::Render();

		draw();

		frameCpuEnd = std::chrono::high_resolution_clock::now();

		{
			std::chrono::duration<double> elapsed_seconds = frameCpuEnd - frameCpuStart;
			_stats.frameCpuAvg = _stats.frameCpuAvg * 0.95 + elapsed_seconds / std::chrono::milliseconds(1) * 0.05;
		}
	}
}

void VulkanEngine::init_vulkan()
{
	VK_CHECK(volkInitialize());

	vkb::InstanceBuilder builder;
	//make the Vulkan instance, with basic debug features
	auto inst_ret = builder.set_app_name("My Vulkan pet project")
		.require_api_version(1, 3, 0)
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

	std::vector<const char*> extensions = {
		VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
#if MESHSHADER_ON
		VK_NV_MESH_SHADER_EXTENSION_NAME,
#endif
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
	};

	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 1)
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

	vkb::Device vkbDevice = deviceBuilder.add_pNext(&shader_draw_parameters_features)
		.add_pNext(&featuresMesh)
		.build().value();

	// Get the VkDevice handle used in the rest of a Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	vkGetPhysicalDeviceProperties(_chosenGPU, &physDevProp);

	// use vkbootstrap to get a Graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
	
	//initialize the memory allocator

	VmaVulkanFunctions vulkanFunctions;
	vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
	vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
	vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
	vulkanFunctions.vkFreeMemory = vkFreeMemory;
	vulkanFunctions.vkMapMemory = vkMapMemory;
	vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
	vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
	vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
	vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
	vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
	vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
	vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
	vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
	vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
	vulkanFunctions.vkCreateImage = vkCreateImage;
	vulkanFunctions.vkDestroyImage = vkDestroyImage;
	vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;


	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _chosenGPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_gpuProperties = vkbDevice.physical_device.properties;

	std::cout << "The GPU has a minimum buffer alignment of " << _gpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;
}

void VulkanEngine::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

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
		.create_texture();
}

void VulkanEngine::init_commands()
{
	//create a command pool for commands submitted to the graphics queue.
	//we also want the pool to allow for resetting of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int i = 0; i < FRAME_OVERLAP; i++) {


		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

		//allocate the default command buffer that we will use for rendering
		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
			});
	}

	VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);
	//create pool for upload context
	VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext._commandPool));

	_mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
		});

	//allocate the default command buffer that we will use for the instant commands
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext._commandPool, 1);

	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_uploadContext._commandBuffer));

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_frames[i].queryPool = createQueryPool(128);
	}
}

void VulkanEngine::init_default_renderpass()
{
	// the renderpass will use this color attachment.
	VkAttachmentDescription color_attachment = {};
	//the attachment will have the format needed by the swapchain
	color_attachment.format = _swapchainImageFormat;
	//1 sample, we won't be doing MSAA
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// we Clear when this attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//we don't care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	//we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	//after the renderpass ends, the image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	//attachment number will index into the pAttachments array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


	VkAttachmentDescription depth_attachment = {};
	// Depth attachment
	depth_attachment.flags = 0;
	depth_attachment.format = _depthFormat;
	depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_attachment_ref = {};
	depth_attachment_ref.attachment = 1;
	depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//we are going to create 1 subpass, which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;
	//hook the depth attachment into the subpass
	subpass.pDepthStencilAttachment = &depth_attachment_ref;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency depth_dependency = {};
	depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	depth_dependency.dstSubpass = 0;
	depth_dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.srcAccessMask = 0;
	depth_dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	depth_dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	//array of 2 attachments, one for the color, and other for depth
	VkAttachmentDescription attachments[2] = { color_attachment,depth_attachment };
	VkSubpassDependency dependencies[2] = { dependency, depth_dependency };

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	//2 attachments from said array
	render_pass_info.attachmentCount = 2;
	render_pass_info.pAttachments = &attachments[0];
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 2;
	render_pass_info.pDependencies = &dependencies[0];

	create_render_pass(render_pass_info, _renderPass);
}

void VulkanEngine::init_framebuffers()
{
	//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = _renderPass;
	fb_info.attachmentCount = 1;
	fb_info.width = _windowExtent.width;
	fb_info.height = _windowExtent.height;
	fb_info.layers = 1;

	//grab how many images we have in the swapchain
	const uint32_t swapchain_imagecount = _swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

	//create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_imagecount; i++) {

		VkImageView attachments[2];
		attachments[0] = _swapchainImageViews[i];
		attachments[1] = _depthTex.imageView;

		fb_info.pAttachments = attachments;
		fb_info.attachmentCount = 2;
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
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

	ShaderEffect defaultEffect;
#if MESHSHADER_ON
	defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.task.spv")), VK_SHADER_STAGE_TASK_BIT_NV);
	defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.mesh.spv")), VK_SHADER_STAGE_MESH_BIT_NV);
	defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);
#else
	defaultEffect.add_stage(_shaderCache.get_shader(shader_path("tri_mesh.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
	defaultEffect.add_stage(_shaderCache.get_shader(shader_path("triangle.frag.spv")), VK_SHADER_STAGE_FRAGMENT_BIT);
#endif
	defaultEffect.reflect_layout(_device, nullptr, 0);
	//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	GraphicPipelineBuilder pipelineBuilder;

	pipelineBuilder.setShaders(&defaultEffect);

	VkPipelineLayout meshPipLayout = pipelineBuilder._pipelineLayout;

	//vertex input controls how to read vertices from vertex buffers. We arent using it yet
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

	//input assembly is the configuration for drawing triangle lists, strips, or individual points.
	//we are just going to draw triangle list
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	//build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)_windowExtent.width;
	pipelineBuilder._viewport.height = (float)_windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;

	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = _windowExtent;

	//configure the rasterizer to draw filled triangles
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	//we dont use multisampling, so just run the default one
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

	//a single blend attachment with no blending and writing to RGBA
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

	//default depthtesting
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	//build the mesh triangle pipeline
#if !MESHSHADER_ON
	pipelineBuilder.vertexDescription = Vertex::get_vertex_description();
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	//connect the pipeline builder vertex input info to the one we get from Vertex
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = pipelineBuilder.vertexDescription.attributes.data();
	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.attributes.size();

	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = pipelineBuilder.vertexDescription.bindings.data();
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.bindings.size();
#endif
	VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(_device, _renderPass);

	load_materials(meshPipeline, meshPipLayout);

	_mainDeletionQueue.push_function([=]() {
		vkDestroyPipeline(_device, meshPipeline, nullptr);

		vkDestroyPipelineLayout(_device, meshPipLayout, nullptr);
		});

#if MESHSHADER_ON
	{
		ShaderEffect computeEffect;
		computeEffect.add_stage(_shaderCache.get_shader(shader_path("drawcmd.comp.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
		computeEffect.reflect_layout(_device, nullptr, 0);

		ComputePipelineBuilder computePipelineBuilder;
		computePipelineBuilder.setShaders(&computeEffect);
		//hook the push constants layout
		_drawcmdPipelineLayout = computePipelineBuilder._pipelineLayout;

		_drawcmdPipeline = computePipelineBuilder.build_compute_pipeline(_device);
	}
#endif

	_depthReduceRenderPass.init_pipelines();
}

void VulkanEngine::load_meshes()
{
	for (auto& mesh: _resManager.meshList)
	{
#if MESHSHADER_ON
		mesh->remapVertexToVertexMS()
			.calcAddInfo()
			.buildMeshlets();
#else
		mesh->calcAddInfo();
#endif // MESHSHADER_ON
		upload_mesh(*mesh);
	}

}

void VulkanEngine::upload_mesh(Mesh& mesh)
{
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
#if MESHSHADER_ON
		{
			size_t bufferSize = mesh._verticesMS.size() * sizeof(Vertex_MS);

			mesh._vertexBuffer[i] = create_buffer_n_copy_data(bufferSize, mesh._verticesMS.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
		{
			size_t bufferSize = mesh._meshlets.size() * sizeof(Meshlet);

			mesh._meshletsBuffer[i] = create_buffer_n_copy_data(bufferSize, mesh._meshlets.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}

		{
			size_t bufferSize = mesh.meshletdata.size() * sizeof(uint32_t);

			mesh._meshletdataBuffer[i] = create_buffer_n_copy_data(bufferSize, mesh.meshletdata.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		}
#else
		{
			size_t bufferSize = mesh._vertices.size() * sizeof(Vertex);

			mesh._vertexBuffer[i] = create_buffer_n_copy_data(bufferSize, mesh._vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		}
		{
			size_t bufferSize = mesh._indices.size() * sizeof(mesh._indices[0]);

			mesh._indicesBuffer[i] = create_buffer_n_copy_data(bufferSize, mesh._indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		}
#endif
	}
}

void VulkanEngine::load_images()
{
	for (auto& [path, texPtr]: _resManager.textureCache)
	{
		Texture* tex = texPtr.get();
		VkFormat image_format;
		if (vkutil::load_image_from_file(*this, path, tex->image, image_format))
		{
			VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(image_format, tex->image._image, VK_IMAGE_ASPECT_COLOR_BIT);
			vkCreateImageView(_device, &imageinfo, nullptr, &tex->imageView);
		}	
	}
}

std::string VulkanEngine::asset_path(std::string_view path)
{
	return "../../assets_export/" + std::string(path);
}

std::string VulkanEngine::shader_path(std::string_view path)
{
	return "../../shaders/" + std::string(path);
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

FrameData& VulkanEngine::get_current_frame()
{
	return _frames[_frameNumber % FRAME_OVERLAP];
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

		resBuffer = create_gpuonly_buffer(allocSize, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		immediate_submit([=](VkCommandBuffer cmd) {
			VkBufferCopy copy;
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = VkDeviceSize(allocSize);
			vkCmdCopyBuffer(cmd, stagingBuffer._buffer, resBuffer._buffer, 1, &copy);
			});

		//add the destruction of mesh buffer to the deletion queue
		_mainDeletionQueue.push_function([=]() {
			vmaDestroyBuffer(_allocator, resBuffer._buffer, resBuffer._allocation);
			});

		vmaDestroyBuffer(_allocator, stagingBuffer._buffer, stagingBuffer._allocation);
	}
	return resBuffer;
}

AllocatedBuffer VulkanEngine::create_gpuonly_buffer(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_buffer(allocSize, usage, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
}

AllocatedBuffer VulkanEngine::create_staging_buffer(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_buffer(allocSize, usage, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |VMA_ALLOCATION_CREATE_MAPPED_BIT);
}

AllocatedBuffer VulkanEngine::create_cpu_to_gpu_buffer(size_t allocSize, VkBufferUsageFlags usage)
{
	return create_staging_buffer(allocSize, usage);
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

	return newBuffer;
}

void VulkanEngine::map_buffer(VmaAllocator& allocator, VmaAllocation& allocation, std::function<void(void*& data)> func)
{
	void* data;
	vmaMapMemory(allocator, allocation, &data);

	func(data);

	vmaUnmapMemory(allocator, allocation);
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

void VulkanEngine::init_descriptors()
{
	//create a descriptor pool that will hold 10 uniform buffers and 10 dynamic uniform buffers
	_descriptorAllocator = std::make_unique<vkutil::DescriptorAllocator>();
	_descriptorAllocator->init(_device);

	_descriptorLayoutCache = std::make_unique<vkutil::DescriptorLayoutCache>();
	_descriptorLayoutCache->init(_device);

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
			vmaDestroyBuffer(_allocator, _frames[i].perframeDataBuffer._buffer, _frames[i].perframeDataBuffer._allocation);
			vmaDestroyBuffer(_allocator, _frames[i].cameraBuffer._buffer, _frames[i].cameraBuffer._allocation);
			vmaDestroyBuffer(_allocator, _frames[i].objectBuffer._buffer, _frames[i].objectBuffer._allocation);
		}
	});

	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		_frames[i].perframeDataBuffer = create_cpu_to_gpu_buffer(sizeof(PerFrameData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		_frames[i].cameraBuffer = create_cpu_to_gpu_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
#if MESHSHADER_ON
		_frames[i].indirectBuffer = create_cpu_to_gpu_buffer(MAX_COMMANDS * sizeof(VkDrawMeshTasksIndirectCommandNV), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
#else
		_frames[i].indirectBuffer = create_cpu_to_gpu_buffer(MAX_COMMANDS * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
#endif
		_frames[i].objectBuffer = create_cpu_to_gpu_buffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		VkDescriptorBufferInfo perframeDataInfo;
		perframeDataInfo.buffer = _frames[i].perframeDataBuffer._buffer;
		perframeDataInfo.offset = 0;
		perframeDataInfo.range = sizeof(PerFrameData);

		VkDescriptorBufferInfo cameraInfo;
		cameraInfo.buffer = _frames[i].cameraBuffer._buffer;
		cameraInfo.offset = 0;
		cameraInfo.range = sizeof(GPUCameraData);
#if MESHSHADER_ON	
		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_buffer(0, &cameraInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV)
			.build(_frames[i].globalDescriptor, _globalSetLayout);
#else
		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_buffer(0, &cameraInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(_frames[i].globalDescriptor, _globalSetLayout);
#endif

		VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = _frames[i].objectBuffer._buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;
#if MESHSHADER_ON

		VkDescriptorBufferInfo indirectBufferInfo;
		indirectBufferInfo.buffer = _frames[i].indirectBuffer._buffer;
		indirectBufferInfo.offset = 0;
		indirectBufferInfo.range = sizeof(VkDrawMeshTasksIndirectCommandNV) * MAX_OBJECTS;

		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(1, &indirectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.bind_buffer(2, &perframeDataInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
			.build(_frames[i].objectDescriptor, _objectSetLayout);
#else
		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_buffer(0, &objectBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build(_frames[i].objectDescriptor, _objectSetLayout);
#endif
	}

	_depthReduceRenderPass.create_depth_pyramid(_windowExtent.width, _windowExtent.height);

	VkDescriptorImageInfo depthDescInfo = {};
	depthDescInfo.imageView = _depthTex.imageView;
	_depthReduceRenderPass.init_descriptors({ DescriptorInfo{ {}, depthDescInfo} });

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
	VkCommandBuffer cmd = _uploadContext._commandBuffer;

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
	vkResetCommandPool(_device, _uploadContext._commandPool, 0);
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

std::vector<IndirectBatch> VulkanEngine::compact_draws(RenderObject* objects, int count)
{
	std::vector<IndirectBatch> draws;

	IndirectBatch firstDraw;
	firstDraw.mesh = objects[0].mesh;
	firstDraw.material = objects[0].material;
	firstDraw.first = 0;
	firstDraw.count = 1;

	draws.push_back(firstDraw);

	for (int i = 0; i < count; i++)
	{
		//compare the mesh and material with the end of the vector of draws
		bool sameMesh = objects[i].mesh == draws.back().mesh;
		bool sameMaterial = objects[i].material == draws.back().material;

		if (sameMesh && sameMaterial)
		{
			//all matches, add count
			draws.back().count++;
		}
		else
		{
			//add new draw
			IndirectBatch newDraw;
			newDraw.mesh = objects[i].mesh;
			newDraw.material = objects[i].material;
			newDraw.transformMatrix = objects[i].transformMatrix;
			newDraw.first = i;
			newDraw.count = 1;

			draws.push_back(newDraw);
		}
	}
	return draws;
}

void VulkanEngine::compute_pass(VkCommandBuffer cmd)
{
#if MESHSHADER_ON
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _drawcmdPipeline);

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _drawcmdPipelineLayout, 0, 1, &get_current_frame().objectDescriptor, 0, nullptr);

	vkCmdDispatch(cmd, uint32_t((_renderables.size() + 31) / 32), 1, 1);

	std::array<VkBufferMemoryBarrier, 2> cullBarriers =
	{
		vkinit::buffer_barrier(get_current_frame().indirectBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
		vkinit::buffer_barrier(get_current_frame().objectBuffer._buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
	};

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, 0, cullBarriers.size(), cullBarriers.data(), 0, 0);
#endif
}

void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first, int count)
{
	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;

	size_t frameIndex = _frameNumber % FRAME_OVERLAP;
#if MESHSHADER_ON

	for (int i = 0; i < count; i++)
	{
		RenderObject& object = first[i];
		//only bind the pipeline if it doesn't match with the already bound one
		lastMaterial = object.material;
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &get_current_frame().globalDescriptor, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &object.mesh->meshletsSet[frameIndex], 0, nullptr);

		VkDeviceSize indirect_offset = i * sizeof(VkDrawMeshTasksIndirectCommandNV);
		uint32_t draw_stride = sizeof(VkDrawMeshTasksIndirectCommandNV);
		vkCmdDrawMeshTasksIndirectNV(cmd, get_current_frame().indirectBuffer._buffer, indirect_offset, 1, draw_stride);

	}
#else

	map_buffer(_allocator, get_current_frame().indirectBuffer._allocation, [&](void*& data) {
		VkDrawIndexedIndirectCommand* drawCommands = (VkDrawIndexedIndirectCommand*)data;
		//encode the draw data of each object into the indirect draw buffer
		for (int i = 0; i < count; i++)
		{
			RenderObject& object = first[i];
			drawCommands[i].indexCount = object.mesh->_indices.size();
			drawCommands[i].instanceCount = 1;
			drawCommands[i].vertexOffset = 0;
			drawCommands[i].firstIndex = 0;
			drawCommands[i].firstInstance = i; //used to access object matrix in the shader
		}
		});

	std::vector<IndirectBatch> draws = compact_draws(first, count);
	for (IndirectBatch& object : draws)
	{
		//only bind the pipeline if it doesn't match with the already bound one
		if (object.material != lastMaterial) {

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &get_current_frame().globalDescriptor, 0, nullptr);

			//object data descriptor
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &get_current_frame().objectDescriptor, 0, nullptr);

			if (object.material->textureSet != VK_NULL_HANDLE) {
				//texture descriptor
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureSet, 0, nullptr);
			}
		}

		//only bind the mesh if its a different one from last bind
		if (object.mesh != lastMesh) {
			//bind the mesh vertex buffer with offset 0
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer[frameIndex]._buffer, &offset);
			vkCmdBindIndexBuffer(cmd, object.mesh->_indicesBuffer[frameIndex]._buffer, offset, VK_INDEX_TYPE_UINT32);
			lastMesh = object.mesh;
		}

		VkDeviceSize indirect_offset = object.first * sizeof(VkDrawIndexedIndirectCommand);
		uint32_t draw_stride = sizeof(VkDrawIndexedIndirectCommand);

		//execute the draw command buffer on each section as defined by the array of draws
		vkCmdDrawIndexedIndirect(cmd, get_current_frame().indirectBuffer._buffer, indirect_offset, object.count, draw_stride);
	}
#endif
}


void VulkanEngine::init_scene()
{
	for (int nodeIndex = 1; nodeIndex < _scene._hierarchy.size(); nodeIndex++)
	{
		RenderObject map;
		int meshIndex = _scene._meshes[nodeIndex];
		map.mesh = _resManager.meshList[meshIndex].get();
		int matDescIndex = _scene._matForNode[nodeIndex];
		const std::string& matName = _resManager.matDescList[matDescIndex].get()->matName;
		map.material = get_material(matName);
		map.transformMatrix = glm::translate(glm::vec3{ 5, -10, 0 });

		_renderables.push_back(map);
	}
	
	for (int i = 0; i < FRAME_OVERLAP; i++)
	{
		map_buffer(_allocator, _frames[i].objectBuffer._allocation, [&](void*& data) {
			GPUObjectData* objectSSBO = (GPUObjectData*)data;

			for (int i = 0; i < _renderables.size(); i++)
			{
				RenderObject& object = _renderables[i];
				objectSSBO[i].modelMatrix = object.transformMatrix;
				objectSSBO[i].center_radius = glm::vec4(object.mesh->_center, object.mesh->_radius);
#if MESHSHADER_ON
				objectSSBO[i].meshletCount = object.mesh->_meshlets.size();
#endif
			}
			});
	}

#if MESHSHADER_ON
	for (auto& mesh : _resManager.meshList)
	{
		for (int i = 0; i < FRAME_OVERLAP; i++)
		{
			VkDescriptorBufferInfo vertexBufferInfo;
			vertexBufferInfo.buffer = mesh->_vertexBuffer[i]._buffer;
			vertexBufferInfo.offset = 0;
			vertexBufferInfo.range = mesh->_vertexBuffer[i]._allocation->GetSize();

			VkDescriptorBufferInfo meshletBufferInfo;
			meshletBufferInfo.buffer = mesh->_meshletsBuffer[i]._buffer;
			meshletBufferInfo.offset = 0;
			meshletBufferInfo.range = mesh->_meshletsBuffer[i]._allocation->GetSize();

			VkDescriptorBufferInfo meshletdataBufferInfo;
			meshletdataBufferInfo.buffer = mesh->_meshletdataBuffer[i]._buffer;
			meshletdataBufferInfo.offset = 0;
			meshletdataBufferInfo.range = mesh->_meshletdataBuffer[i]._allocation->GetSize();

			VkDescriptorImageInfo depthPyramidImageBufferInfo;
			depthPyramidImageBufferInfo.sampler = _depthReduceRenderPass.get_depthSampl();
			depthPyramidImageBufferInfo.imageView = _depthReduceRenderPass.get_depthPyramidTex().imageView;
			depthPyramidImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
				.bind_buffer(0, &vertexBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV)
				.bind_buffer(1, &meshletBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV)
				.bind_buffer(2, &meshletdataBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MESH_BIT_NV)
				.bind_image(3, &depthPyramidImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_TASK_BIT_NV)
				.build(mesh->meshletsSet[i], _meshletsSetLayout);
		}
	}
#else
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler blockySampler;
	vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler);

	for (const auto& matDesc : _resManager.matDescList)
	{
		if (matDesc->diffuseTexture.empty())
			continue;

		const std::string& matName = matDesc->matName;
		Material* texturedMat = get_material(matName);

		VkDescriptorImageInfo imageBufferInfo;
		imageBufferInfo.sampler = blockySampler;
		imageBufferInfo.imageView = _resManager.textureCache[matDesc->diffuseTexture]->imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkutil::DescriptorBuilder::begin(_descriptorLayoutCache.get(), _descriptorAllocator.get())
			.bind_image(0, &imageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(texturedMat->textureSet, _singleTextureSetLayout);
	}
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
	init_info.PhysicalDevice = _chosenGPU;
	init_info.Device = _device;
	init_info.Queue = _graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_LoadFunctions([](const char* function_name, void* vulkan_instance) {
		return vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(vulkan_instance)), function_name);
		}, &_instance);

	ImGui_ImplVulkan_Init(&init_info, _renderPass);

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



