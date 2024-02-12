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

	for (uint32_t pipelineIndex = 0; pipelineIndex < instanceDesc.pipelinesNum; pipelineIndex++)
	{
		const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[pipelineIndex];

		std::string fileName = vk_utils::NRD_SHADERS_PATH + nrdPipelineDesc.shaderFileName;
		ShaderLoader::hlsl_to_spirv_cross_compiler(fileName.c_str(), "cs", "main", { "NRD_COMPILER_DXC = 1",  "NRD_NORMAL_ENCODING = 2", "NRD_ROUGHNESS_ENCODING = 1" }, nullptr);
	}
}


void VulkanRaytracerDenoiserPass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{

}

#endif
