#include <graphic_pipeline/vk_raytracer_denoiser_pass.h>

#if GI_RAYTRACER_ON

#include <vk_engine.h>
#include <vk_framebuffer.h>
#include <vk_command_buffer.h>
#include <vk_render_pipeline.h>
#include <vk_material_system.h>
#include <vk_shaders.h>
#include <vk_raytracer_builder.h>
#include <vk_initializers.h>
#include <sys_config/vk_strings.h>

namespace NrdConfig {
	
	#define SAMPLER_SHIFT 0 // sampler (read-only) aka VK_DESCRIPTOR_TYPE_SAMPLER - using s# registers 
	#define	SRV_SHIFT 128 // shader resource view (read-only) aka VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE - using t# registers
	#define	CBV_SHIFT 256 // constant buffer view (read-only) aka VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER - using b# registers
	#define UAV_SHIFT 384 // unordered access view (read-write) aka VK_DESCRIPTOR_TYPE_STORAGE_IMAGE - using u# registers

	#define  kMaxSceneDistance                  50000.0         // used as a general max distance between any two surface points in the scene, excluding environment map - should be less than kMaxRayTravel; 50k is within fp16 floats; note: actual sceneLength can be longer due to bounces.
	#define  kMaxRayTravel                      (1e15f)         // one AU is ~1.5e11; 1e15 is high enough to use as environment map distance to avoid parallax but low enough to avoid precision issues with various packing and etc.

	#define  cStablePlaneCount                  (3u)            // more than 3 is not supported although 4 could be supported if needed (with some reshuffling)

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

	init_denoiser_textures();

	const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);

	m_ConstantBuffer = _engine->create_cpu_to_gpu_buffer(instanceDesc.constantBufferMaxDataSize * instanceDesc.descriptorPoolDesc.setsMaxNum * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	for (uint32_t samplerIndex = 0; samplerIndex < instanceDesc.samplersNum; samplerIndex++)
	{
		const nrd::Sampler& samplerMode = instanceDesc.samplers[samplerIndex];

		ESamplerType samplType;

		switch (samplerMode)
		{
		case nrd::Sampler::NEAREST_CLAMP:
			samplType = ESamplerType::NEAREST_CLAMP;
			break;
		case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
			samplType = ESamplerType::NEAREST_MIRRORED_REPEAT;
			break;
		case nrd::Sampler::LINEAR_CLAMP:
			samplType = ESamplerType::LINEAR_CLAMP;
			break;
		case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
			samplType = ESamplerType::LINEAR_MIRRORED_REPEAT;
			break;
		default:
			assert(!"Unknown NRD sampler mode");
			break;
		}

		m_Samplers.push_back(samplType);
	}
	
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

				_pipelineBindingLayouts.push_back({ .shaderName = fileName, .pipType = denoiserPipType });
				_pipelineBindingLayouts.back().bindings.clear();
				for (const auto& [_, bind] : computeEffect.bindings)
				{
					_pipelineBindingLayouts.back().bindings[bind.type].push_back({ .set = bind.set, .binding = bind.binding });
					_pipelineBindingLayouts.back().writeDescSet.push_back(
						{
							.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							.dstBinding = bind.binding,
							.descriptorCount = 1,
							.descriptorType = bind.type,
						});
				}
			});
	}
}

void VulkanRaytracerDenoiserPass::set_params(DenoiserParams&& params)
{
	_params = params;
}

static inline void MatrixToNrd(float* dest, const glm::mat4& m)
{
	glm::mat4 tm = glm::transpose(m);
	memcpy(dest, &m, sizeof(m));
}


void VulkanRaytracerDenoiserPass::draw(VulkanCommandBuffer* cmd, int current_frame_index)
{
	nrd::CommonSettings commonSettings;
	MatrixToNrd(commonSettings.worldToViewMatrix, _engine->_camera.get_view_matrix());
	MatrixToNrd(commonSettings.worldToViewMatrixPrev, _engine->_camera.get_prev_view_matrix());
	MatrixToNrd(commonSettings.viewToClipMatrix, _engine->_camera.get_projection_matrix(false));
	MatrixToNrd(commonSettings.viewToClipMatrixPrev, _engine->_camera.get_prev_projection_matrix(false));

	glm::vec2 pixelOffset = _engine->_camera.get_current_jitter();
	glm::vec2 prevPixelOffset = _engine->_camera.get_prev_jitter();
	commonSettings.isMotionVectorInWorldSpace = false;
	commonSettings.motionVectorScale[0] = (commonSettings.isMotionVectorInWorldSpace) ? (1.f) : (1.f / _engine->_windowExtent.width);
	commonSettings.motionVectorScale[1] = (commonSettings.isMotionVectorInWorldSpace) ? (1.f) : (1.f / _engine->_windowExtent.height);
	commonSettings.motionVectorScale[2] = 1.0f;
	commonSettings.cameraJitter[0] = pixelOffset.x;
	commonSettings.cameraJitter[1] = pixelOffset.y;
	commonSettings.cameraJitterPrev[0] = prevPixelOffset.x;
	commonSettings.cameraJitterPrev[1] = prevPixelOffset.y;
	commonSettings.frameIndex = current_frame_index;
	commonSettings.denoisingRange = kMaxSceneDistance * 2;   // with various bounces (in non-primary planes or with PSR) the virtual view Z can be much longer, so adding 2x!
	commonSettings.enableValidation = _params.enableValidation;
	commonSettings.disocclusionThreshold = _params.disocclusionThreshold;
	commonSettings.disocclusionThresholdAlternate = _params.disocclusionThresholdAlternate;
	commonSettings.isDisocclusionThresholdMixAvailable = _params.useDisocclusionThresholdAlternateMix;
	commonSettings.timeDeltaBetweenFrames = _params.timeDeltaBetweenFrames;

	nrd::SetCommonSettings(*m_Instance, commonSettings);

	const nrd::DispatchDesc* dispatchDescs = nullptr;
	uint32_t dispatchDescNum = 0;
	nrd::GetComputeDispatches(*m_Instance, &m_Identifier, 1, dispatchDescs, dispatchDescNum);

	const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);
	for (uint32_t dispatchIndex = 0; dispatchIndex < dispatchDescNum; dispatchIndex++)
	{
		const nrd::DispatchDesc& dispatchDesc = dispatchDescs[dispatchIndex];

		PipelineDesc& pipDesc = _pipelineBindingLayouts[dispatchDesc.pipelineIndex];

		if (pipDesc.bindings.find(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) != pipDesc.bindings.end())
		{
			_engine->write_buffer(_engine->_allocator, m_ConstantBuffer._allocation, dispatchDesc.constantBufferData, dispatchDesc.constantBufferDataSize);

			pipDesc.bindings[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER][0].bufferInfo = {
				.buffer = m_ConstantBuffer._buffer,
				.offset = 0,
				.range = m_ConstantBuffer._size
			};

			for (size_t i = 0; i < pipDesc.writeDescSet.size(); i++)
			{
				if (pipDesc.writeDescSet[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				{
					pipDesc.writeDescSet[i].pBufferInfo = &pipDesc.bindings[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER][0].bufferInfo;
					break;
				}
			}
		}

		if (pipDesc.bindings.find(VK_DESCRIPTOR_TYPE_SAMPLER) != pipDesc.bindings.end())
		{
			for (size_t i = 0; i < pipDesc.bindings[VK_DESCRIPTOR_TYPE_SAMPLER].size(); i++)
			{
				ESamplerType smpType = m_Samplers[pipDesc.bindings[VK_DESCRIPTOR_TYPE_SAMPLER][i].binding];
				pipDesc.bindings[VK_DESCRIPTOR_TYPE_SAMPLER][i].imageInfo = {
					.sampler = _engine->get_engine_sampler(smpType)->sampler
				};		
			}

			size_t samplerCount = 0;
			for (size_t i = 0; i < pipDesc.writeDescSet.size(); i++)
			{
				if (pipDesc.writeDescSet[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
				{
					pipDesc.writeDescSet[i].pImageInfo = &pipDesc.bindings[VK_DESCRIPTOR_TYPE_SAMPLER][samplerCount].imageInfo;
					samplerCount++;
				}
			}
		}

		const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[dispatchDesc.pipelineIndex];
		uint32_t resourceIndex = 0;

		std::vector<VkDescriptorImageInfo> imageInfoList = {};

		for (uint32_t descriptorRangeIndex = 0; descriptorRangeIndex < nrdPipelineDesc.resourceRangesNum; descriptorRangeIndex++)
		{
			const nrd::ResourceRangeDesc& nrdDescriptorRange = nrdPipelineDesc.resourceRanges[descriptorRangeIndex];

			for (uint32_t descriptorOffset = 0; descriptorOffset < nrdDescriptorRange.descriptorsNum; descriptorOffset++)
			{
				assert(resourceIndex < dispatchDesc.resourcesNum);
				const nrd::ResourceDesc& resource = dispatchDesc.resources[resourceIndex];

				assert(resource.stateNeeded == nrdDescriptorRange.descriptorType);

				Texture* texture;
				switch (resource.type)
				{
				case nrd::ResourceType::IN_MV:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserMotionVectors);
					break;
				case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserNormalRoughness);
					break;
				case nrd::ResourceType::IN_VIEWZ:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserViewspaceZ);
					break;
				case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserSpecRadianceHitDist);
					break;
				case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserDiffRadianceHitDist);
					break;
				case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
				{
					ETextureResourceNames rtName = static_cast<ETextureResourceNames>(static_cast<uint32_t>(ETextureResourceNames::DenoiserOutSpecRadianceHitDist_0) + _params.pass);
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserDiffRadianceHitDist);
				}
					break;
				case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
				{
					ETextureResourceNames rtName = static_cast<ETextureResourceNames>(static_cast<uint32_t>(ETextureResourceNames::DenoiserOutDiffRadianceHitDist_0) + _params.pass);
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserDiffRadianceHitDist);
				}
					break;
				case nrd::ResourceType::OUT_VALIDATION:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserOutValidation);
					break;
				case nrd::ResourceType::IN_DISOCCLUSION_THRESHOLD_MIX:
					texture = _engine->get_engine_texture(ETextureResourceNames::DenoiserDisocclusionThresholdMix);
					break;
				case nrd::ResourceType::TRANSIENT_POOL:
					texture = &m_TransientTextures[resource.indexInPool];
					break;
				case nrd::ResourceType::PERMANENT_POOL:
					texture = &m_PermanentTextures[resource.indexInPool];
					break;
				default:
					assert(!"Unavailable resource type");
					break;
				}

				assert(texture);

				if (pipDesc.bindings.find(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) != pipDesc.bindings.end())
				{
					uint32_t bindingIndex = nrdDescriptorRange.baseRegisterIndex + descriptorOffset;

					pipDesc.bindings[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE][bindingIndex].imageInfo = {
						.imageView = texture->imageView,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					};

					for (size_t i = 0; i < pipDesc.writeDescSet.size(); i++)
					{
						if (pipDesc.writeDescSet[i].dstBinding == bindingIndex + SRV_SHIFT)
						{
							pipDesc.writeDescSet[i].pImageInfo = &pipDesc.bindings[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE][bindingIndex].imageInfo;
							break;
						}
					}
				} 
				else if (pipDesc.bindings.find(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) != pipDesc.bindings.end())
				{
					uint32_t bindingIndex = nrdDescriptorRange.baseRegisterIndex + descriptorOffset;

					pipDesc.bindings[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE][bindingIndex].imageInfo = {
						.imageView = texture->imageView,
						.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
					};

					for (size_t i = 0; i < pipDesc.writeDescSet.size(); i++)
					{
						if (pipDesc.writeDescSet[i].dstBinding == bindingIndex + UAV_SHIFT)
						{
							pipDesc.writeDescSet[i].pImageInfo = &pipDesc.bindings[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE][bindingIndex].imageInfo;
							break;
						}
					}
				}

				resourceIndex++;
			}
		}

		assert(resourceIndex == dispatchDesc.resourcesNum);

		cmd->dispatch(dispatchDesc.gridWidth, dispatchDesc.gridHeight, 1, [&](VkCommandBuffer cmd) {
			  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipeline(pipDesc.pipType));
			  vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _engine->_renderPipelineManager.get_pipelineLayout(pipDesc.pipType), 0, pipDesc.writeDescSet.size(), pipDesc.writeDescSet.data());
			});
	}
}

void VulkanRaytracerDenoiserPass::init_denoiser_textures()
{
	VkExtent3D imageExtent = {
		_engine->_windowExtent.width, 
		_engine->_windowExtent.height,
		1
	};

	VulkanTextureBuilder texBuilder;
	texBuilder.init(_engine);

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserMotionVectors);
	}

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserNormalRoughness);
	}

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserViewspaceZ);
	}

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserSpecRadianceHitDist);

		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutSpecRadianceHitDist_0);

		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutSpecRadianceHitDist_1);

		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutSpecRadianceHitDist_2);
	}

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserDiffRadianceHitDist);

		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutDiffRadianceHitDist_0);

		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutDiffRadianceHitDist_1);

		texBuilder.start()
			.make_img_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutDiffRadianceHitDist_2);
	}

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserOutValidation);
	}

	{
		texBuilder.start()
			.make_img_info(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageExtent)
			.fill_img_info([=](VkImageCreateInfo& imgInfo) { imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; })
			.make_img_allocinfo(VMA_MEMORY_USAGE_GPU_ONLY, VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			.make_view_info(VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT)
			.create_engine_texture(ETextureResourceNames::DenoiserDisocclusionThresholdMix);
	}
}

#endif
