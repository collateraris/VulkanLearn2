#include <graphic_pipeline/vk_flux_generation.h>

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>

void VulkanFluxGeneration::init(VulkanEngine* engine)
{
	_engine = engine;

	_imageExtent = {
		_engine->_windowExtent.width,
		_engine->_windowExtent.height,
		1
	};

	{
		init_description_set_global_buffer();
	}

	{
		_engine->_renderPipelineManager.init_render_pipeline(_engine, EPipelineType::Generate_Flux,
			[=](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {
				ShaderEffect computeEffect;
				computeEffect.add_stage(_engine->_shaderCache.get_shader(VulkanEngine::shader_slang_path("generate_flux.comp.slang.spv")), VK_SHADER_STAGE_COMPUTE_BIT);
				computeEffect.reflect_layout(engine->_device, nullptr, 0);

				ComputePipelineBuilder computePipelineBuilder;
				computePipelineBuilder.setShaders(&computeEffect);

				VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();
				std::vector<VkDescriptorSetLayout> setLayout = { _engine->get_engine_descriptor(EDescriptorResourceNames::FluxData)->setLayout };
				mesh_pipeline_layout_info.setLayoutCount = setLayout.size();
				mesh_pipeline_layout_info.pSetLayouts = setLayout.data();

				vkCreatePipelineLayout(_engine->_device, &mesh_pipeline_layout_info, nullptr, &computePipelineBuilder._pipelineLayout);
				//hook the push constants layout
				pipelineLayout = computePipelineBuilder._pipelineLayout;

				pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
			});
	}
}

void VulkanFluxGeneration::init_description_set_global_buffer()
{
	_fluxData = _engine->create_cpu_to_gpu_buffer(sizeof(VulkanFluxGeneration::SFluxData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	VkDescriptorBufferInfo globalUniformsInfo;
	globalUniformsInfo.buffer = _fluxData._buffer;
	globalUniformsInfo.offset = 0;
	globalUniformsInfo.range = VK_WHOLE_SIZE;

	const uint32_t textureBinding = 1;
	const uint32_t blockySamplerBinding = 2;
	const uint32_t lightBufferBinding = 3;
	const uint32_t materialBinding = 4;

	//BIND SAMPLERS
	VkSampler& blockySampler = _engine->get_engine_sampler(ESamplerType::LINEAR_REPEAT)->sampler;


	std::vector<VkDescriptorImageInfo> imageInfoList{};
	imageInfoList.resize(_engine->_resManager.textureList.size());

	for (uint32_t textureArrayIndex = 0; textureArrayIndex < _engine->_resManager.textureList.size(); textureArrayIndex++)
	{
		VkDescriptorImageInfo& imageBufferInfo = imageInfoList[textureArrayIndex];
		imageBufferInfo.sampler = blockySampler;
		imageBufferInfo.imageView = _engine->_resManager.textureList[textureArrayIndex]->imageView;
		imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}

	VkDescriptorImageInfo blockySamplerInfo;
	blockySamplerInfo.sampler = blockySampler;
	blockySamplerInfo.imageView = nullptr;
	blockySamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorBufferInfo lightsInfo;
	lightsInfo.buffer = _engine->_lightManager.get_light_buffer()._buffer;
	lightsInfo.offset = 0;
	lightsInfo.range = VK_WHOLE_SIZE;

	VkDescriptorBufferInfo matBufferInfo;
	matBufferInfo.buffer = _engine->_resManager.globalMaterialBuffer._buffer;
	matBufferInfo.offset = 0;
	matBufferInfo.range = VK_WHOLE_SIZE;

	vkutil::DescriptorBuilder::begin(_engine->_descriptorLayoutCache.get(), _engine->_descriptorAllocator.get())
		.bind_buffer(0, &globalUniformsInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_image(textureBinding, imageInfoList.data(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, imageInfoList.size())
		.bind_image(blockySamplerBinding, &blockySamplerInfo, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_buffer(lightBufferBinding, &lightsInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.bind_buffer(materialBinding, &matBufferInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(_engine, EDescriptorResourceNames::FluxData);
}
void VulkanFluxGeneration::draw(VkCommandBuffer cmd, uint32_t lights_num)
{
	SFluxData fluxData = { lights_num, 0, 0, 0};
	_engine->map_buffer(_engine->_allocator, _fluxData._allocation, [&](void*& data) {
		memcpy(data, &fluxData, sizeof(VulkanFluxGeneration::SFluxData));
		});

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipeline(EPipelineType::Generate_Flux));

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(EPipelineType::Generate_Flux), 0,
		1, &_engine->get_engine_descriptor(EDescriptorResourceNames::FluxData)->set, 0, nullptr);

	uint32_t groupCount = (lights_num + 255) / 256;
	vkCmdDispatch(cmd, groupCount, 1, 1);
}