#include "vk_render_passes.h"

#include <vk_engine.h>
#include <vk_textures.h>
//bootstrap library
#include "VkBootstrap.h"
#include <vk_initializers.h>

void VulkanMainRenderPass::init(VulkanEngine* engine)
{
	_engine = engine;
}

void VulkanMainRenderPass::init_render_pass()
{
	init_swapchain();
	init_depth_buffer();
	init_framebuffers();

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

	_engine->create_render_pass(render_pass_info, _render_pass);
}

const VkFramebuffer& VulkanMainRenderPass::get_framebuffer(size_t index) const
{
	return _framebuffers.at(index);
}

const VkRenderPass& VulkanMainRenderPass::get_render_pass() const
{
	return _render_pass;
}

const VkSwapchainKHR& VulkanMainRenderPass::get_swap_chain() const
{
	return _swapchain;
}

void VulkanMainRenderPass::init_swapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{_engine->_chosenPhysicalDeviceGPU, _engine->_device, _engine->_surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_engine->_windowExtent.width, _engine->_windowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;

	_engine->_mainDeletionQueue.push_function([=]() {
		vkDestroySwapchainKHR(_engine->_device, _swapchain, nullptr);
		});
}

void VulkanMainRenderPass::init_depth_buffer()
{
	//depth image size will match the window
	VkExtent3D depthImageExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	VulkanTextureBuilder texBuilder;
	texBuilder.init(_engine);
	_depthTex = texBuilder.start()
		.make_img_info(_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent)
		.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		.make_view_info(_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT)
		.create_texture();
}

void VulkanMainRenderPass::init_framebuffers()
{
	//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = _render_pass;
	fb_info.attachmentCount = 1;
	fb_info.width = _engine->_windowExtent.width;
	fb_info.height = _engine->_windowExtent.height;
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
		vkCreateFramebuffer(_engine->_device, &fb_info, nullptr, &_framebuffers[i]);

		_engine->_mainDeletionQueue.push_function([=]() {
			vkDestroyFramebuffer(_engine->_device, _framebuffers[i], nullptr);
			vkDestroyImageView(_engine->_device, _swapchainImageViews[i], nullptr);
			});
	}
}

void VulkanDepthReduceRenderPass::init(VulkanEngine* engine)
{
	_engine = engine;
}

void VulkanDepthReduceRenderPass::init_render_pass()
{
}

void VulkanDepthReduceRenderPass::init_pipelines()
{
	//hook the push constants layout
	_drawcmdPipelineLayout = _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::PyramidDepthReduce);

	_drawcmdPipeline = _engine->_renderPipelineManager.get_pipeline(EPipelineType::PyramidDepthReduce);
}

void VulkanDepthReduceRenderPass::init_descriptors(const std::vector<DescriptorInfo>& descInfo)
{
	//another set, one that holds a single texture
	VkDescriptorSetLayoutBinding outTextureBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0);
	VkDescriptorSetLayoutBinding inTextureBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1);

	std::vector<VkDescriptorSetLayoutBinding> binding = { outTextureBind, inTextureBind };

	VkDescriptorSetLayoutCreateInfo set1info = {};
	set1info.bindingCount = binding.size();
	set1info.flags = 0;
	set1info.pNext = nullptr;
	set1info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set1info.pBindings = binding.data();

	_objectSetLayout = _engine->_descriptorLayoutCache->create_descriptor_layout(&set1info);

	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

	createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;

	samplerInfo.pNext = &createInfoReduction;

	_depthSampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &_depthSampler);

	_depthDescInfo = descInfo[0].imageInfo;
	_depthDescInfo.sampler = _depthSampler;
	_depthDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	_depthPyramidDescInfo.resize(_depthPyramidLevels);
	for (uint32_t i = 0; i < _depthPyramidLevels; ++i)
	{
		_depthPyramidDescInfo[i].imageView = depthPyramidMips[i];
		_depthPyramidDescInfo[i].sampler = _depthSampler;
		_depthPyramidDescInfo[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	for (size_t f = 0; f < FRAME_OVERLAP; ++f)
	{
		_objectDescriptor[f].resize(_depthPyramidLevels);
		for (uint32_t i = 0; i < _depthPyramidLevels; ++i)
		{
			VkDescriptorImageInfo& writeImage = _depthPyramidDescInfo[i];
			VkDescriptorImageInfo& sourceImage = i == 0 ? _depthDescInfo : _depthPyramidDescInfo[i - 1];

			vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
				.bind_image(0, &writeImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
				.bind_image(1, &sourceImage, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
				.build(_objectDescriptor[f][i], _objectSetLayout);
		}
	}
}

void VulkanDepthReduceRenderPass::create_depth_pyramid(size_t w, size_t h)
{
	_width = vkutil::previousPow2(w);
	_height = vkutil::previousPow2(w);

	_depthPyramidLevels = vkutil::getImageMipLevels(_width, _height);

	VkExtent3D depthImageExtent = {
		_width,
		_height,
		1
	};

	VulkanTextureBuilder texBuilder;
	texBuilder.init(_engine);

	depthPyramid = texBuilder.start()
		.make_img_info(VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, depthImageExtent)
		.fill_img_info([&](VkImageCreateInfo& imgInfo) {
					imgInfo.mipLevels = _depthPyramidLevels;
			})
		.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		.make_view_info(VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
		.fill_view_info([&](VkImageViewCreateInfo& view_info) {
				view_info.subresourceRange.baseMipLevel = 0;
				view_info.subresourceRange.levelCount = _depthPyramidLevels;
				})
		.create_texture();

	for (uint32_t i = 0; i < _depthPyramidLevels; ++i)
	{
		depthPyramidMips[i] = texBuilder.fill_view_info([&](VkImageViewCreateInfo& view_info) {
			view_info.subresourceRange.baseMipLevel = i;
			view_info.subresourceRange.levelCount = 1;
			})
			.create_image_view(depthPyramid.image);
	}
}

const Texture& VulkanDepthReduceRenderPass::get_depthPyramidTex() const
{
	return depthPyramid;
}

const VkSampler& VulkanDepthReduceRenderPass::get_depthSampl() const
{
	return _depthSampler;
}

void VulkanDepthReduceRenderPass::compute_pass(VkCommandBuffer cmd, size_t index, const std::vector<Resources>& resource)
{
	Texture* depthTex = resource[0].texture;

	std::array<VkImageMemoryBarrier, 2> depthReadBarriers =
	{
		vkinit::image_barrier(depthTex->image._image, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
		vkinit::image_barrier(depthPyramid.image._image, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
	};

	depthReadBarriers[1].subresourceRange.baseMipLevel = 0;

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, depthReadBarriers.size(), depthReadBarriers.data());

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _drawcmdPipeline);

	for (uint32_t i = 0; i < _depthPyramidLevels; ++i)
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _drawcmdPipelineLayout, 0, 1, &_objectDescriptor[index][i], 0, nullptr);

		uint32_t levelWidth = std::max(1u, (_width) >> i);
		uint32_t levelHeight = std::max(1u, (_height) >> i);

		DepthReduceData reduceData = { vec2(levelWidth, levelHeight) };

		vkCmdPushConstants(cmd, _drawcmdPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);

		vkCmdDispatch(cmd, (levelWidth + 32 - 1) / 32, (levelHeight + 32 - 1) / 32, 1);

		if (i + 1 < _depthPyramidLevels)
		{
			std::array<VkImageMemoryBarrier, 2> reduceBarrier =
			{
				vkinit::image_barrier(depthPyramid.image._image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
				vkinit::image_barrier(depthPyramid.image._image, 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT)
			};

			reduceBarrier[0].subresourceRange.baseMipLevel = i;
			reduceBarrier[1].subresourceRange.baseMipLevel = i + 1;

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, reduceBarrier.size(), reduceBarrier.data());
		}
	}

	VkImageMemoryBarrier depthWriteBarrier = vkinit::image_barrier(depthTex->image._image, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &depthWriteBarrier);
}
