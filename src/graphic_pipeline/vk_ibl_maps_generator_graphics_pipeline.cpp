#include "vk_ibl_maps_generator_graphics_pipeline.h"

#include <vk_engine.h>
#include <vk_textures.h>
#include <vk_initializers.h>

#include <sys_config/ConfigManager.h>
#include <sys_config/vk_strings.h>

#include <vk_scene.h>
#include <vk_assimp_loader.h>

void VulkanIblMapsGeneratorGraphicsPipeline::init(VulkanEngine* engine, std::string hdrCubemapPath)
{
	_engine = engine;

	loadEnvironment(hdrCubemapPath);
	loadBoxMesh();
	createOffscreenFramebuffer();
	drawCubemaps();
}

const Texture& VulkanIblMapsGeneratorGraphicsPipeline::getHDR() const
{
	return _hdr;
}

const Texture& VulkanIblMapsGeneratorGraphicsPipeline::getEnvCubemap() const
{
	return _environmentCube;
}

void VulkanIblMapsGeneratorGraphicsPipeline::loadEnvironment(std::string hdrCubemapPath)
{
	{
		_hdr.flags = ETexFlags::NO_MIPS | ETexFlags::HDR_CUBEMAP;
		VkFormat image_format;
		if (vkutil::load_image_from_file(*_engine, hdrCubemapPath, _hdr, image_format))
		{
			VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(image_format, _hdr.image._image, VK_IMAGE_ASPECT_COLOR_BIT);
			imageinfo.subresourceRange.levelCount = _hdr.mipLevels;
			vkCreateImageView(_engine->_device, &imageinfo, nullptr, &_hdr.imageView);
		}
	}

	_imageExtent = {
		vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize(),
		vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize(),
		1
	};

	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_environmentCube = texBuilder.start()
			.make_cubemap_img_info(_colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_cubemap_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();
	}
}

void VulkanIblMapsGeneratorGraphicsPipeline::loadBoxMesh()
{
	Scene _scene;
	SceneConfig config;
	config.fileName = vk_utils::BOX_MODEL;
	AsimpLoader::processScene(config, _scene, _engine->_resManager, config.model);

	std::unordered_map<int, std::unordered_map<int, std::vector<RenderObject>>> renderablesMap;

	for (int nodeIndex = 1; nodeIndex < _scene._hierarchy.size(); nodeIndex++)
	{
		RenderObject map;
		map.meshIndex = _scene._meshes[nodeIndex];
		map.mesh = _engine->_resManager.meshList[map.meshIndex].get();
		map.matDescIndex = _scene._matForNode[nodeIndex];
		map.transformMatrix = _scene._localTransforms[nodeIndex];

		renderablesMap[map.meshIndex][map.matDescIndex].push_back(map);
	}

	_box.clear();
	for (const auto& [meshIndex, matMap] : renderablesMap)
		for (const auto& [matIndex, mapVector] : matMap)
			for (const auto& map : mapVector)
				_box.push_back(map);

	RenderObject& cube = _box[0];

	{
		size_t bufferSize = cube.mesh->_vertices.size() * sizeof(Vertex);

		cube.mesh->_vertexBuffer = _engine->create_buffer_n_copy_data(bufferSize, cube.mesh->_vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	}
	{
		size_t bufferSize = cube.mesh->_indices.size() * sizeof(cube.mesh->_indices[0]);

		cube.mesh->_indicesBuffer = _engine->create_buffer_n_copy_data(bufferSize, cube.mesh->_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	}

}

void VulkanIblMapsGeneratorGraphicsPipeline::createOffscreenFramebuffer()
{
	{
		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		_offscreenRT = texBuilder.start()
			.make_img_info(_colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, _imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { 
				imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			})
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(_colorFormat, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();

		_engine->immediate_submit([&](VkCommandBuffer cmd) {

			std::array<VkImageMemoryBarrier, 1> offscreenBarriers =
			{
				vkinit::image_barrier(_offscreenRT.image._image, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
			};

			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, offscreenBarriers.size(), offscreenBarriers.data());

		});
	}

	//create render pass
	{
		RenderPassInfo default_rp;
		default_rp.op_flags = RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
		default_rp.clear_attachments = BIT(0);
		default_rp.store_attachments = BIT(0);
		default_rp.num_color_attachments = 1;
		default_rp.color_attachments[0] = &_offscreenRT;
		default_rp.depth_stencil = nullptr;

		RenderPassInfo::Subpass subpass = {};
		subpass.num_color_attachments = 1;
		subpass.depth_stencil_mode = RenderPassInfo::DepthStencil::None;
		subpass.color_attachments[0] = 0;

		default_rp.num_subpasses = 1;
		default_rp.subpasses = &subpass;

		_engine->_renderPassManager.get_render_pass(ERenderPassType::DrawIntoCubemap)->init(_engine, default_rp);
	}

	{
		VkFramebufferCreateInfo fb_info = {};
		fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb_info.pNext = nullptr;

		fb_info.renderPass = _engine->_renderPassManager.get_render_pass(ERenderPassType::DrawIntoCubemap)->get_render_pass();
		fb_info.width = _imageExtent.width;
		fb_info.height = _imageExtent.height;
		fb_info.layers = 1;

		std::array<VkImageView, 1> attachments;
		attachments[0] = _offscreenRT.imageView;

		fb_info.pAttachments = attachments.data();
		fb_info.attachmentCount = attachments.size();
		vkCreateFramebuffer(_engine->_device, &fb_info, nullptr, &_framebuffer);
	}
}

void VulkanIblMapsGeneratorGraphicsPipeline::drawCubemaps()
{
	drawHDRtoEnvMap();
}

void init_render_pipeline(VulkanEngine* engine, EPipelineType rpType, std::string_view fragShader, uint32_t cubemapSize)
{
	engine->_renderPipelineManager.init_render_pipeline(engine, rpType,
		[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
			ShaderEffect defaultEffect;
			defaultEffect.add_stage(engine->_shaderCache.get_shader(VulkanEngine::shader_path("filtercube.vert.spv")), VK_SHADER_STAGE_VERTEX_BIT);
			defaultEffect.add_stage(engine->_shaderCache.get_shader(VulkanEngine::shader_path(fragShader)), VK_SHADER_STAGE_FRAGMENT_BIT);

			defaultEffect.reflect_layout(engine->_device, nullptr, 0);
			//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
			GraphicPipelineBuilder pipelineBuilder;

			pipelineBuilder.setShaders(&defaultEffect);
			VkPipelineLayout meshPipLayout = pipelineBuilder._pipelineLayout;

			//vertex input controls how to read vertices from vertex buffers. We arent using it yet
			pipelineBuilder.vertexDescription = Vertex::get_vertex_description();
			pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

			pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = pipelineBuilder.vertexDescription.bindings.data();
			pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)pipelineBuilder.vertexDescription.bindings.size();

			//input assembly is the configuration for drawing triangle lists, strips, or individual points.
			//we are just going to draw triangle list
			pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

			//build viewport and scissor from the swapchain extents
			pipelineBuilder._viewport.x = 0.0f;
			pipelineBuilder._viewport.y = 0.0f;
			pipelineBuilder._viewport.width = cubemapSize;
			pipelineBuilder._viewport.height = cubemapSize;
			pipelineBuilder._viewport.minDepth = 0.0f;
			pipelineBuilder._viewport.maxDepth = 1.0f;

			pipelineBuilder._scissor.offset = { 0, 0 };
			pipelineBuilder._scissor.extent = VkExtent2D(cubemapSize, cubemapSize);

			//configure the rasterizer to draw filled triangles
			pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

			//we dont use multisampling, so just run the default one
			pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

			pipelineBuilder.attachment_count = 1;
			pipelineBuilder._colorBlendAttachment.push_back(vkinit::color_blend_attachment_state());

			//default depthtesting
			pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);

			VkPipeline meshPipeline = pipelineBuilder.build_graphic_pipeline(engine->_device,
				engine->_renderPassManager.get_render_pass(ERenderPassType::DrawIntoCubemap)->get_render_pass());

			pipeline = meshPipeline;
			pipelineLayout = meshPipLayout;
		});
}

void VulkanIblMapsGeneratorGraphicsPipeline::drawHDRtoEnvMap()
{
	init_render_pipeline(_engine, EPipelineType::DrawHDRtoEnvMap, "drawHDRtoEnvMap.frag.spv",
		vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize());

	VkDescriptorSetLayout          _descSetLayout;
	VkDescriptorSet  _descSet;

	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VkSamplerReductionModeCreateInfoEXT createInfoReduction = { VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT };

	createInfoReduction.reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

	samplerInfo.pNext = &createInfoReduction;

	VkSampler sampler;
	vkCreateSampler(_engine->_device, &samplerInfo, nullptr, &sampler);

	VkDescriptorImageInfo hdrImageBufferInfo;
	hdrImageBufferInfo.sampler = sampler;
	hdrImageBufferInfo.imageView = _hdr.imageView;
	hdrImageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_image(0, &hdrImageBufferInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(_descSet, _descSetLayout);

	_engine->immediate_submit([&](VkCommandBuffer cmd) {

		std::array<VkImageMemoryBarrier, 1> envMapBarriers =
		{
			vkinit::image_barrier(_environmentCube.image._image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, envMapBarriers.size(), envMapBarriers.data());

	});

	// Render cubemap
	VkClearValue clearValues[1];
	clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

	VkViewport viewport{};
	viewport.width = (float)vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize();
	viewport.height = (float)vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize();
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.extent.width = vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize();
	scissor.extent.height = vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize();

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 6;

	struct PushBlockEnvMap {
		glm::mat4 mvp;
	} pushBlockEnvMap;

	for (uint32_t cubeFace = 0; cubeFace < 6; cubeFace++)
	{
		_engine->immediate_submit([&](VkCommandBuffer cmd) {

			vkCmdSetViewport(cmd, 0, 1, &viewport);
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			//start the main renderpass. 
			//We will use the clear color from above, and the framebuffer of the index the swapchain gave us
			uint32_t windowSize = vk_utils::ConfigManager::Get().GetConfig(vk_utils::MAIN_CONFIG_PATH).GetEnvMapSize();
			VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_engine->_renderPassManager.get_render_pass(ERenderPassType::DrawIntoCubemap)->get_render_pass(), VkExtent2D(windowSize, windowSize), _framebuffer);

			//connect clear values
			rpInfo.clearValueCount = 1;
			rpInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipeline(EPipelineType::DrawHDRtoEnvMap));

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::DrawHDRtoEnvMap), 0, 1, &_descSet, 0, nullptr);

			pushBlockEnvMap.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * _viewMatrices[cubeFace];
			vkCmdPushConstants(cmd, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::DrawHDRtoEnvMap), 
				VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushBlockEnvMap), &pushBlockEnvMap);

			drawCube(cmd);

			vkCmdEndRenderPass(cmd);

			{
				std::array<VkImageMemoryBarrier, 1> offscreenBarriers =
				{
					vkinit::image_barrier(_offscreenRT.image._image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
				};

				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, offscreenBarriers.size(), offscreenBarriers.data());
			}

			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion{};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = cubeFace;
			copyRegion.dstSubresource.mipLevel = 0;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
			copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(
				cmd,
				_offscreenRT.image._image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				_environmentCube.image._image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

			{
				std::array<VkImageMemoryBarrier, 1> offscreenBarriers =
				{
					vkinit::image_barrier(_offscreenRT.image._image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
				};

				vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, offscreenBarriers.size(), offscreenBarriers.data());
			}
		});
	}

	_engine->immediate_submit([&](VkCommandBuffer cmd) {

		std::array<VkImageMemoryBarrier, 1> envMapBarriers =
		{
			vkinit::image_barrier(_environmentCube.image._image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
		};

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, envMapBarriers.size(), envMapBarriers.data());

	});
}

void VulkanIblMapsGeneratorGraphicsPipeline::drawCube(VkCommandBuffer& cmd)
{
	RenderObject& cube = _box[0];

	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &cube.mesh->_vertexBuffer._buffer, offsets);

	vkCmdBindIndexBuffer(cmd, cube.mesh->_indicesBuffer._buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, static_cast<uint32_t>(cube.mesh->_indices.size()), 1, 0, 0, 0);
}
