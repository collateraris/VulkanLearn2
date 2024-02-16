#include <graphic_pipeline/vk_raytracer_denoiser_pass.h>

#if GI_RAYTRACER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <sys_config/vk_strings.h>

namespace NrdConfig {

	nrd::RelaxDiffuseSpecularSettings getDefaultRELAXSettings() {

		nrd::RelaxDiffuseSpecularSettings settings;

		settings.enableAntiFirefly = true;

		settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::OFF;
		// (pixels) - pre-accumulation spatial reuse pass blur radius (0 = disabled, must be used in case of probabilistic sampling) <- we're using probabilistic sampling
		settings.diffusePrepassBlurRadius = 0.0f;
		settings.specularPrepassBlurRadius = 0.0f;  // <- using prepass blur causes more issues than it solves

		settings.atrousIterationNum = 5; // 5 is default; 4 gives better shadows but more boiling, 6 gives less boiling but loss in contact shadows

		settings.specularLobeAngleFraction = 0.65f;
		settings.specularLobeAngleSlack = 0.35f;         // good to hide noisy secondary bounces

		settings.depthThreshold = 0.004f;

		settings.diffuseMaxAccumulatedFrameNum = 50;
		settings.specularMaxAccumulatedFrameNum = 50;

		settings.antilagSettings.accelerationAmount = 0.95f;
		settings.antilagSettings.spatialSigmaScale = 0.85f;
		settings.antilagSettings.temporalSigmaScale = 0.15f;
		settings.antilagSettings.resetAmount = 0.95f;


		return settings;
	}

	nrd::ReblurSettings getDefaultREBLURSettings() {

		nrd::ReblurSettings settings;
		settings.enableAntiFirefly = true;
		settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_5X5;
		settings.maxAccumulatedFrameNum = 50;

		// reducing prepass blurs to reduce loss of sharp shadows
		settings.diffusePrepassBlurRadius = 15.0f;
		settings.specularPrepassBlurRadius = 40.0f;

		return settings;
	}
}

static void* NrdAllocate(void* userArg, size_t size, size_t alignment)
{
	return malloc(size);
}

static void* NrdReallocate(void* userArg, void* memory, size_t size, size_t alignment)
{
	return realloc(memory, size);
}

static void NrdFree(void* userArg, void* memory)
{
	free(memory);
}

ESamplerType get_vk_engine_sampler_mode(nrd::Sampler nrdSamplerMode)
{
	switch (nrdSamplerMode)
	{
	case nrd::Sampler::NEAREST_CLAMP:
		return ESamplerType::NEAREST_CLAMP;
	case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
		return ESamplerType::NEAREST_MIRRORED_REPEAT;
	case nrd::Sampler::LINEAR_CLAMP:
		return ESamplerType::LINEAR_CLAMP;
	case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
		return ESamplerType::LINEAR_MIRRORED_REPEAT;
	default:
		assert(!"Unknown NRD sampler mode");
		return ESamplerType::NEAREST_CLAMP;
	}
}

static VkFormat GetVulkanFormat(nrd::Format format)
{
	switch (format)
	{
	case nrd::Format::R8_UNORM:             return VkFormat::VK_FORMAT_R8_UNORM;
	case nrd::Format::R8_SNORM:             return VkFormat::VK_FORMAT_R8_SNORM;
	case nrd::Format::R8_UINT:              return VkFormat::VK_FORMAT_R8_UINT;
	case nrd::Format::R8_SINT:              return VkFormat::VK_FORMAT_R8_SINT;
	case nrd::Format::RG8_UNORM:            return VkFormat::VK_FORMAT_R8G8_UNORM;
	case nrd::Format::RG8_SNORM:            return VkFormat::VK_FORMAT_R8G8_SNORM;
	case nrd::Format::RG8_UINT:             return VkFormat::VK_FORMAT_R8G8_UINT;
	case nrd::Format::RG8_SINT:             return VkFormat::VK_FORMAT_R8G8_SINT;
	case nrd::Format::RGBA8_UNORM:          return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	case nrd::Format::RGBA8_SNORM:          return VkFormat::VK_FORMAT_R8G8B8A8_SNORM;
	case nrd::Format::RGBA8_UINT:           return VkFormat::VK_FORMAT_R8G8B8A8_UINT;
	case nrd::Format::RGBA8_SINT:           return VkFormat::VK_FORMAT_R8G8B8A8_SINT;
	case nrd::Format::RGBA8_SRGB:           return VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
	case nrd::Format::R16_UNORM:            return VkFormat::VK_FORMAT_R16_UNORM;
	case nrd::Format::R16_SNORM:            return VkFormat::VK_FORMAT_R16_SNORM;
	case nrd::Format::R16_UINT:             return VkFormat::VK_FORMAT_R16_UINT;
	case nrd::Format::R16_SINT:             return VkFormat::VK_FORMAT_R16_SINT;
	case nrd::Format::R16_SFLOAT:           return VkFormat::VK_FORMAT_R16_SFLOAT;
	case nrd::Format::RG16_UNORM:           return VkFormat::VK_FORMAT_R16G16_UNORM;
	case nrd::Format::RG16_SNORM:           return VkFormat::VK_FORMAT_R16G16_SNORM;
	case nrd::Format::RG16_UINT:            return VkFormat::VK_FORMAT_R16G16_UINT;
	case nrd::Format::RG16_SINT:            return VkFormat::VK_FORMAT_R16G16_SINT;
	case nrd::Format::RG16_SFLOAT:          return VkFormat::VK_FORMAT_R16G16_SFLOAT;
	case nrd::Format::RGBA16_UNORM:         return VkFormat::VK_FORMAT_R16G16B16A16_UNORM;
	case nrd::Format::RGBA16_SNORM:         return VkFormat::VK_FORMAT_R16G16B16A16_SNORM;
	case nrd::Format::RGBA16_UINT:          return VkFormat::VK_FORMAT_R16G16B16A16_UINT;
	case nrd::Format::RGBA16_SINT:          return VkFormat::VK_FORMAT_R16G16B16A16_SINT;
	case nrd::Format::RGBA16_SFLOAT:        return VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT;
	case nrd::Format::R32_UINT:             return VkFormat::VK_FORMAT_R32_UINT;
	case nrd::Format::R32_SINT:             return VkFormat::VK_FORMAT_R32_SINT;
	case nrd::Format::R32_SFLOAT:           return VkFormat::VK_FORMAT_R32_SFLOAT;
	case nrd::Format::RG32_UINT:            return VkFormat::VK_FORMAT_R32G32_UINT;
	case nrd::Format::RG32_SINT:            return VkFormat::VK_FORMAT_R32G32_SINT;
	case nrd::Format::RG32_SFLOAT:          return VkFormat::VK_FORMAT_R32G32_SFLOAT;
	case nrd::Format::RGB32_UINT:           return VkFormat::VK_FORMAT_R32G32B32_UINT;
	case nrd::Format::RGB32_SINT:           return VkFormat::VK_FORMAT_R32G32B32_SINT;
	case nrd::Format::RGB32_SFLOAT:         return VkFormat::VK_FORMAT_R32G32B32_SFLOAT;
	case nrd::Format::RGBA32_UINT:          return VkFormat::VK_FORMAT_R32G32B32A32_UINT;
	case nrd::Format::RGBA32_SINT:          return VkFormat::VK_FORMAT_R32G32B32A32_SINT;
	case nrd::Format::RGBA32_SFLOAT:        return VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT;
	case nrd::Format::R10_G10_B10_A2_UNORM: return VkFormat::VK_FORMAT_A2R10G10B10_UNORM_PACK32;
	case nrd::Format::R10_G10_B10_A2_UINT:  return VkFormat::VK_FORMAT_UNDEFINED; // not representable and not used
	case nrd::Format::R11_G11_B10_UFLOAT:   return VkFormat::VK_FORMAT_UNDEFINED;
	case nrd::Format::R9_G9_B9_E5_UFLOAT:   return VkFormat::VK_FORMAT_UNDEFINED; // not representable and not used
	default:                                return VkFormat::VK_FORMAT_UNDEFINED;
	}
}

void VulkanRaytracerDenoiserPass::init(VulkanEngine* engine)
{
	_engine = engine;

	const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();

	const std::array<nrd::DenoiserDesc, 1> denoiserDescs =
	{
		{ m_Identifier, m_Denoiser, uint16_t(_engine->_windowExtent.width), uint16_t(_engine->_windowExtent.height)}
	};

	nrd::InstanceCreationDesc instanceCreationDesc;
	instanceCreationDesc.memoryAllocatorInterface.Allocate = NrdAllocate;
	instanceCreationDesc.memoryAllocatorInterface.Reallocate = NrdReallocate;
	instanceCreationDesc.memoryAllocatorInterface.Free = NrdFree;
	instanceCreationDesc.denoisers = denoiserDescs.data();
	instanceCreationDesc.denoisersNum = denoiserDescs.size();

	nrd::Result res = nrd::CreateInstance(instanceCreationDesc, m_Instance);
	if (res != nrd::Result::SUCCESS)
	{
		PLOG_DEBUG << "NRD Raytrace Denoiser CreateInstance failed";
		return;
	}

	const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);

	m_ConstantBuffer = _engine->create_cpu_to_gpu_buffer(instanceDesc.constantBufferMaxDataSize * instanceDesc.descriptorPoolDesc.setsMaxNum * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	for (uint32_t i = 0; i < instanceDesc.permanentPoolSize; i++)
	{
		const nrd::TextureDesc& nrdTextureDesc = instanceDesc.permanentPool[i];

		const VkFormat format = GetVulkanFormat(nrdTextureDesc.format);

		if (format == VkFormat::VK_FORMAT_UNDEFINED)
		{
			assert(!"Unknown or unsupported NRD format");
			continue;
		}

		VkExtent3D imageExtent = {
			nrdTextureDesc.width,
			nrdTextureDesc.height,
			1
		};


		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		const Texture texture = texBuilder.start()
			.make_img_info(format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, imageExtent)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(format, VK_IMAGE_ASPECT_DEPTH_BIT)
			.create_texture();

		m_PermanentTextures.push_back(texture);
	}

	for (uint32_t i = 0; i < instanceDesc.transientPoolSize; i++)
	{
		const nrd::TextureDesc& nrdTextureDesc = instanceDesc.transientPool[i];

		const VkFormat format = GetVulkanFormat(nrdTextureDesc.format);

		if (format == VkFormat::VK_FORMAT_UNDEFINED)
		{
			assert(!"Unknown or unsupported NRD format");
			continue;
		}

		VkExtent3D imageExtent = {
			nrdTextureDesc.width,
			nrdTextureDesc.height,
			1
		};

		VulkanTextureBuilder texBuilder;
		texBuilder.init(_engine);
		const Texture texture = texBuilder.start()
			.make_img_info(format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, imageExtent)
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(format, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_texture();

		m_TransientTextures.push_back(texture);
	}

	for (uint32_t pipelineIndex = 0; pipelineIndex < instanceDesc.pipelinesNum; pipelineIndex++)
	{
		const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[pipelineIndex];

		std::string fileName = vk_utils::NRD_SHADERS_PATH + nrdPipelineDesc.shaderFileName;
		EPipelineType denoiserPipType = static_cast<EPipelineType>(static_cast<uint32_t>(EPipelineType::NRD_RaytraceDenoiseShader_0) + pipelineIndex);
		_engine->_renderPipelineManager.init_render_pipeline(_engine, denoiserPipType,
			[&](VkPipeline& pipeline, VkPipelineLayout& pipelineLayout) {

				ShaderEffect computeEffect;
				computeEffect.add_stage(_engine->_shaderCache.get_shader(fileName + ".hlsl.spv"), VK_SHADER_STAGE_COMPUTE_BIT);
				computeEffect.reflect_layout(_engine->_device, nullptr, 0);

				ComputePipelineBuilder computePipelineBuilder;
				computePipelineBuilder.setShaders(&computeEffect);
				//hook the push constants layout
				pipelineLayout = computePipelineBuilder._pipelineLayout;

				pipeline = computePipelineBuilder.build_compute_pipeline(engine->_device);
			});
	}
}


void VulkanRaytracerDenoiserPass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{

}

#endif
