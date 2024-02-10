/*
* Copyright (c) 2023, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "NrdIntegration.h"

#if WITH_NRD

static_assert(NRD_VERSION_MAJOR >= 4 && NRD_VERSION_MINOR >= 0, "Unsupported NRD version!");

#include "../RenderTargets.h"
#include <nvrhi/utils.h>
#include <donut/core/math/math.h>
#include <donut/engine/View.h>
#include <donut/engine/ShaderFactory.h>
#include <sstream>
#include <donut/core/log.h>

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

static nvrhi::Format GetNvrhiFormat(nrd::Format format)
{
    switch (format)
    {
    case nrd::Format::R8_UNORM:             return nvrhi::Format::R8_UNORM;
    case nrd::Format::R8_SNORM:             return nvrhi::Format::R8_SNORM;
    case nrd::Format::R8_UINT:              return nvrhi::Format::R8_UINT;
    case nrd::Format::R8_SINT:              return nvrhi::Format::R8_SINT;
    case nrd::Format::RG8_UNORM:            return nvrhi::Format::RG8_UNORM;
    case nrd::Format::RG8_SNORM:            return nvrhi::Format::RG8_SNORM;
    case nrd::Format::RG8_UINT:             return nvrhi::Format::RG8_UINT;
    case nrd::Format::RG8_SINT:             return nvrhi::Format::RG8_SINT;
    case nrd::Format::RGBA8_UNORM:          return nvrhi::Format::RGBA8_UNORM;
    case nrd::Format::RGBA8_SNORM:          return nvrhi::Format::RGBA8_SNORM;
    case nrd::Format::RGBA8_UINT:           return nvrhi::Format::RGBA8_UINT;
    case nrd::Format::RGBA8_SINT:           return nvrhi::Format::RGBA8_SINT;
    case nrd::Format::RGBA8_SRGB:           return nvrhi::Format::SRGBA8_UNORM;
    case nrd::Format::R16_UNORM:            return nvrhi::Format::R16_UNORM;
    case nrd::Format::R16_SNORM:            return nvrhi::Format::R16_SNORM;
    case nrd::Format::R16_UINT:             return nvrhi::Format::R16_UINT;
    case nrd::Format::R16_SINT:             return nvrhi::Format::R16_SINT;
    case nrd::Format::R16_SFLOAT:           return nvrhi::Format::R16_FLOAT;
    case nrd::Format::RG16_UNORM:           return nvrhi::Format::RG16_UNORM;
    case nrd::Format::RG16_SNORM:           return nvrhi::Format::RG16_SNORM;
    case nrd::Format::RG16_UINT:            return nvrhi::Format::RG16_UINT;
    case nrd::Format::RG16_SINT:            return nvrhi::Format::RG16_SINT;
    case nrd::Format::RG16_SFLOAT:          return nvrhi::Format::RG16_FLOAT;
    case nrd::Format::RGBA16_UNORM:         return nvrhi::Format::RGBA16_UNORM;
    case nrd::Format::RGBA16_SNORM:         return nvrhi::Format::RGBA16_SNORM;
    case nrd::Format::RGBA16_UINT:          return nvrhi::Format::RGBA16_UINT;
    case nrd::Format::RGBA16_SINT:          return nvrhi::Format::RGBA16_SINT;
    case nrd::Format::RGBA16_SFLOAT:        return nvrhi::Format::RGBA16_FLOAT;
    case nrd::Format::R32_UINT:             return nvrhi::Format::R32_UINT;
    case nrd::Format::R32_SINT:             return nvrhi::Format::R32_SINT;
    case nrd::Format::R32_SFLOAT:           return nvrhi::Format::R32_FLOAT;
    case nrd::Format::RG32_UINT:            return nvrhi::Format::RG32_UINT;
    case nrd::Format::RG32_SINT:            return nvrhi::Format::RG32_SINT;
    case nrd::Format::RG32_SFLOAT:          return nvrhi::Format::RG32_FLOAT;
    case nrd::Format::RGB32_UINT:           return nvrhi::Format::RGB32_UINT;
    case nrd::Format::RGB32_SINT:           return nvrhi::Format::RGB32_SINT;
    case nrd::Format::RGB32_SFLOAT:         return nvrhi::Format::RGB32_FLOAT;
    case nrd::Format::RGBA32_UINT:          return nvrhi::Format::RGBA32_UINT;
    case nrd::Format::RGBA32_SINT:          return nvrhi::Format::RGBA32_SINT;
    case nrd::Format::RGBA32_SFLOAT:        return nvrhi::Format::RGBA32_FLOAT;
    case nrd::Format::R10_G10_B10_A2_UNORM: return nvrhi::Format::R10G10B10A2_UNORM;
    case nrd::Format::R10_G10_B10_A2_UINT:  return nvrhi::Format::UNKNOWN; // not representable and not used
    case nrd::Format::R11_G11_B10_UFLOAT:   return nvrhi::Format::R11G11B10_FLOAT;
    case nrd::Format::R9_G9_B9_E5_UFLOAT:   return nvrhi::Format::UNKNOWN; // not representable and not used
    default:                                return nvrhi::Format::UNKNOWN;
    }
}

NrdIntegration::NrdIntegration(nvrhi::IDevice* device, nrd::Denoiser denoiser)
    : m_Device(device)
    , m_Initialized(false)
    , m_Instance(nullptr)
    , m_Denoiser(denoiser)
    , m_BindingCache(device)
    , m_Identifier(0)
{
}

NrdIntegration::~NrdIntegration()
{
    if (m_Initialized)
    {
        nrd::DestroyInstance(*m_Instance);
    }
}

bool NrdIntegration::Initialize(uint32_t width, uint32_t height, donut::engine::ShaderFactory& shaderFactory)
{
    const nrd::LibraryDesc& libraryDesc = nrd::GetLibraryDesc();

    const nrd::DenoiserDesc denoiserDescs[] =
    {
        { m_Identifier, m_Denoiser, uint16_t(width), uint16_t(height)}
    };

    nrd::InstanceCreationDesc instanceCreationDesc;
    instanceCreationDesc.memoryAllocatorInterface.Allocate = NrdAllocate;
    instanceCreationDesc.memoryAllocatorInterface.Reallocate = NrdReallocate;
    instanceCreationDesc.memoryAllocatorInterface.Free = NrdFree;
    instanceCreationDesc.denoisers = denoiserDescs;
    instanceCreationDesc.denoisersNum = dim(denoiserDescs);

    nrd::Result res = nrd::CreateInstance(instanceCreationDesc, m_Instance);
    if (res != nrd::Result::SUCCESS)
        return false;

    const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance); 

    const nvrhi::BufferDesc constantBufferDesc = nvrhi::utils::CreateVolatileConstantBufferDesc(
        instanceDesc.constantBufferMaxDataSize,
        "NrdConstantBuffer", 
        instanceDesc.descriptorPoolDesc.setsMaxNum * 4);

    m_ConstantBuffer = m_Device->createBuffer(constantBufferDesc);

    for (uint32_t samplerIndex = 0; samplerIndex < instanceDesc.samplersNum; samplerIndex++)
    {
        const nrd::Sampler& samplerMode = instanceDesc.samplers[samplerIndex];

        nvrhi::SamplerAddressMode addressMode = nvrhi::SamplerAddressMode::Wrap;
        bool filter = false;

        switch (samplerMode)
        {
        case nrd::Sampler::NEAREST_CLAMP:
            addressMode = nvrhi::SamplerAddressMode::Clamp;
            filter = false;
            break;
        case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
            addressMode = nvrhi::SamplerAddressMode::Mirror;
            filter = false;
            break;
        case nrd::Sampler::LINEAR_CLAMP:
            addressMode = nvrhi::SamplerAddressMode::Clamp;
            filter = true;
            break;
        case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
            addressMode = nvrhi::SamplerAddressMode::Mirror;
            filter = true;
            break;
        default:
            assert(!"Unknown NRD sampler mode");
            break;
        }

        auto samplerDesc = nvrhi::SamplerDesc()
            .setAllAddressModes(addressMode)
            .setAllFilters(filter);

        const nvrhi::SamplerHandle sampler = m_Device->createSampler(samplerDesc);

        if (!sampler)
        {
            assert(!"Cannot create an NRD sampler");
            return false;
        }

        m_Samplers.push_back(sampler);
    }

    for (uint32_t pipelineIndex = 0; pipelineIndex < instanceDesc.pipelinesNum; pipelineIndex++)
    {
        const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[pipelineIndex];

        std::string fileName = std::string("nrd/RayTracingDenoiser/Shaders/Source/") + nrdPipelineDesc.shaderFileName;
        std::vector<donut::engine::ShaderMacro> macros = { {"NRD_COMPILER_DXC", "1"}, {"NRD_NORMAL_ENCODING", "2"}, {"NRD_ROUGHNESS_ENCODING", "1"} };
        
        NrdPipeline pipeline;
        pipeline.Shader = shaderFactory.CreateShader(fileName.c_str(), "main", &macros, nvrhi::ShaderType::Compute);

        if (!pipeline.Shader)
        {
            assert(!"Cannot create an NRD shader");
            return false;
        }

        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::Compute;

        nvrhi::BindingLayoutItem constantBufferItem = {};
        constantBufferItem.type = nvrhi::ResourceType::VolatileConstantBuffer;
        constantBufferItem.slot = instanceDesc.constantBufferRegisterIndex;
        layoutDesc.bindings.push_back(constantBufferItem);

        assert(instanceDesc.samplersSpaceIndex == 0);
        for (uint32_t samplerIndex = 0; samplerIndex < instanceDesc.samplersNum; samplerIndex++)
        {
            //const nrd::StaticSamplerDesc& nrdStaticSampler = denoiserDesc.samplers[samplerIndex];

            nvrhi::BindingLayoutItem samplerItem = {};
            samplerItem.type = nvrhi::ResourceType::Sampler;
            samplerItem.slot = instanceDesc.samplersBaseRegisterIndex + samplerIndex;
            layoutDesc.bindings.push_back(samplerItem);
        }

        for (uint32_t descriptorRangeIndex = 0; descriptorRangeIndex < nrdPipelineDesc.resourceRangesNum; descriptorRangeIndex++)
        {
            const nrd::ResourceRangeDesc& nrdDescriptorRange = nrdPipelineDesc.resourceRanges[descriptorRangeIndex];

            nvrhi::BindingLayoutItem resourceItem = {};
            switch (nrdDescriptorRange.descriptorType)
            {
            case nrd::DescriptorType::TEXTURE:
                resourceItem.type = nvrhi::ResourceType::Texture_SRV;
                break;
            case nrd::DescriptorType::STORAGE_TEXTURE:
                resourceItem.type = nvrhi::ResourceType::Texture_UAV;
                break;
            default:
                assert(!"Unknown NRD descriptor type");
                break;
            }

            for (uint32_t descriptorOffset = 0; descriptorOffset < nrdDescriptorRange.descriptorsNum; descriptorOffset++)
            {
                resourceItem.slot = nrdDescriptorRange.baseRegisterIndex + descriptorOffset;
                layoutDesc.bindings.push_back(resourceItem);
            }
        }

        pipeline.BindingLayout = m_Device->createBindingLayout(layoutDesc);

        if (!pipeline.BindingLayout)
        {
            assert(!"Cannot create an NRD binding layout");
            return false;
        }

        nvrhi::ComputePipelineDesc pipelineDesc;
        pipelineDesc.bindingLayouts = { pipeline.BindingLayout };
        pipelineDesc.CS = pipeline.Shader;
        pipeline.Pipeline = m_Device->createComputePipeline(pipelineDesc);

        if (!pipeline.Pipeline)
        {
            assert(!"Cannot create an NRD pipeline");
            return false;
        }

        m_Pipelines.push_back(pipeline);
    }


    const uint32_t poolSize = instanceDesc.permanentPoolSize + instanceDesc.transientPoolSize;

    for (uint32_t i = 0; i < poolSize; i++)
    {
        const bool isPermanent = (i < instanceDesc.permanentPoolSize);

        const nrd::TextureDesc& nrdTextureDesc = isPermanent 
            ? instanceDesc.permanentPool[i]
            : instanceDesc.transientPool[i - instanceDesc.permanentPoolSize];

        const nvrhi::Format format = GetNvrhiFormat(nrdTextureDesc.format);

        if (format == nvrhi::Format::UNKNOWN)
        {
            assert(!"Unknown or unsupported NRD format");
            return false;
        }

        std::stringstream ss;
        ss << "NRD " << (isPermanent ? "Permanent" : "Transient") << "Texture [" << (isPermanent ? i : i - instanceDesc.permanentPoolSize) << "]";

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = nrdTextureDesc.width;
        textureDesc.height = nrdTextureDesc.height;
        textureDesc.format = format;
        textureDesc.mipLevels = nrdTextureDesc.mipNum;
        textureDesc.dimension = nvrhi::TextureDimension::Texture2D;
        textureDesc.initialState = nvrhi::ResourceStates::ShaderResource;
        textureDesc.keepInitialState = true;
        textureDesc.isUAV = true;
        textureDesc.debugName = ss.str();

        const nvrhi::TextureHandle texture = m_Device->createTexture(textureDesc);

        if (!texture)
        {
            assert(!"Cannot create an NRD texture");
            return false;
        }

        if (isPermanent)
            m_PermanentTextures.push_back(texture);
        else
            m_TransientTextures.push_back(texture);
    }

    m_Initialized = true;
    return true;
}

bool NrdIntegration::IsAvailable() const
{
    return m_Initialized;
}

static inline void MatrixToNrd(float* dest, const dm::float4x4& m)
{
    dm::float4x4 tm = dm::transpose(m);
    memcpy(dest, &m, sizeof(m));
}

void NrdIntegration::RunDenoiserPasses(
    nvrhi::ICommandList* commandList,
    const RenderTargets& renderTargets,
    int pass,
    const donut::engine::PlanarView& view, 
    const donut::engine::PlanarView& viewPrev, 
    uint32_t frameIndex,
    float disocclusionThreshold,
    float disocclusionThresholdAlternate,
    bool useDisocclusionThresholdAlternateMix,
    float timeDeltaBetweenFrames,
    bool enableValidation,
    const void* methodSettings)
{
    if (methodSettings)
    {
        nrd::SetDenoiserSettings(*m_Instance, m_Identifier, methodSettings);
    }

    nrd::CommonSettings commonSettings;
    MatrixToNrd(commonSettings.worldToViewMatrix, dm::affineToHomogeneous(view.GetViewMatrix()));           //MatrixToNrd(commonSettings.worldToViewRotationMatrix, dm::affineToHomogeneous(view.GetViewMatrix()));
    MatrixToNrd(commonSettings.worldToViewMatrixPrev, dm::affineToHomogeneous(viewPrev.GetViewMatrix()));   //MatrixToNrd(commonSettings.worldToViewRotationMatrixPrev, dm::affineToHomogeneous(viewPrev.GetViewMatrix()));
    MatrixToNrd(commonSettings.viewToClipMatrix, view.GetProjectionMatrix(false));
    MatrixToNrd(commonSettings.viewToClipMatrixPrev, viewPrev.GetProjectionMatrix(false));

    dm::float2 pixelOffset = view.GetPixelOffset();
    dm::float2 prevPixelOffset = viewPrev.GetPixelOffset();
    commonSettings.isMotionVectorInWorldSpace = false;
    commonSettings.motionVectorScale[0] = (commonSettings.isMotionVectorInWorldSpace)?(1.f):(1.f / view.GetViewExtent().width());
    commonSettings.motionVectorScale[1] = (commonSettings.isMotionVectorInWorldSpace)?(1.f):(1.f / view.GetViewExtent().height());
    commonSettings.motionVectorScale[2] = 1.0f;
    commonSettings.cameraJitter[0] = pixelOffset.x;
    commonSettings.cameraJitter[1] = pixelOffset.y;
    commonSettings.cameraJitterPrev[0] = prevPixelOffset.x;
    commonSettings.cameraJitterPrev[1] = prevPixelOffset.y;
    commonSettings.frameIndex = frameIndex;
    commonSettings.denoisingRange = kMaxSceneDistance * 2;   // with various bounces (in non-primary planes or with PSR) the virtual view Z can be much longer, so adding 2x!
    commonSettings.enableValidation = enableValidation && renderTargets.DenoiserOutValidation != nullptr;
    commonSettings.disocclusionThreshold = disocclusionThreshold;
    commonSettings.disocclusionThresholdAlternate = disocclusionThresholdAlternate;
    commonSettings.isDisocclusionThresholdMixAvailable = useDisocclusionThresholdAlternateMix;
    commonSettings.timeDeltaBetweenFrames = timeDeltaBetweenFrames;

    nrd::SetCommonSettings(*m_Instance, commonSettings);

    const nrd::DispatchDesc* dispatchDescs = nullptr;
    uint32_t dispatchDescNum = 0;
    nrd::GetComputeDispatches(*m_Instance, &m_Identifier, 1, dispatchDescs, dispatchDescNum);

    const nrd::InstanceDesc& instanceDesc = nrd::GetInstanceDesc(*m_Instance);

    for (uint32_t dispatchIndex = 0; dispatchIndex < dispatchDescNum; dispatchIndex++)
    {
        const nrd::DispatchDesc& dispatchDesc = dispatchDescs[dispatchIndex];

        if (dispatchDesc.name)
        {
            commandList->beginMarker(dispatchDesc.name);
        }

        assert(m_ConstantBuffer);
        commandList->writeBuffer(m_ConstantBuffer, dispatchDesc.constantBufferData, dispatchDesc.constantBufferDataSize);

        nvrhi::BindingSetDesc setDesc;
        setDesc.bindings.push_back(nvrhi::BindingSetItem::ConstantBuffer(instanceDesc.constantBufferRegisterIndex, m_ConstantBuffer));

        for (uint32_t samplerIndex = 0; samplerIndex < instanceDesc.samplersNum; samplerIndex++)
        {
            assert(m_Samplers[samplerIndex]);
            setDesc.bindings.push_back(nvrhi::BindingSetItem::Sampler(instanceDesc.samplersBaseRegisterIndex + samplerIndex, m_Samplers[samplerIndex]));
        }

        const nrd::PipelineDesc& nrdPipelineDesc = instanceDesc.pipelines[dispatchDesc.pipelineIndex];
        uint32_t resourceIndex = 0;

        for (uint32_t descriptorRangeIndex = 0; descriptorRangeIndex < nrdPipelineDesc.resourceRangesNum; descriptorRangeIndex++)
        {
            const nrd::ResourceRangeDesc& nrdDescriptorRange = nrdPipelineDesc.resourceRanges[descriptorRangeIndex];

            for (uint32_t descriptorOffset = 0; descriptorOffset < nrdDescriptorRange.descriptorsNum; descriptorOffset++)
            {
                assert(resourceIndex < dispatchDesc.resourcesNum);
                const nrd::ResourceDesc& resource = dispatchDesc.resources[resourceIndex];

                assert(resource.stateNeeded == nrdDescriptorRange.descriptorType);

                nvrhi::TextureHandle texture;
                switch (resource.type)
                {
                case nrd::ResourceType::IN_MV:
                    texture = renderTargets.DenoiserMotionVectors;
                    break;
                case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
                    texture = renderTargets.DenoiserNormalRoughness;
                    break;
                case nrd::ResourceType::IN_VIEWZ:
                    texture = renderTargets.DenoiserViewspaceZ;
                    break;
                case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
                    texture = renderTargets.DenoiserSpecRadianceHitDist;
                    break;
                case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
                    texture = renderTargets.DenoiserDiffRadianceHitDist;
                    break;
                case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
                    texture = renderTargets.DenoiserOutSpecRadianceHitDist[pass];
                    break;
                case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
                    texture = renderTargets.DenoiserOutDiffRadianceHitDist[pass];
                    break;
                case nrd::ResourceType::OUT_VALIDATION:
                    texture = renderTargets.DenoiserOutValidation;
                    break;
                //case nrd::ResourceType::IN_DIFF_HIT:
                //    texture = renderTargets.DiffuseLighting;
                //    break;
                //case nrd::ResourceType::IN_SPEC_HIT:
                //    texture = renderTargets.SpecularLighting;
                //    break;
                //case nrd::ResourceType::OUT_DIFF_HIT:
                //    texture = renderTargets.DenoisedDiffuseLighting;
                //    break;
                //case nrd::ResourceType::OUT_SPEC_HIT:
                //    texture = renderTargets.DenoisedSpecularLighting;
                //    break;
                case nrd::ResourceType::IN_DISOCCLUSION_THRESHOLD_MIX:
                    texture = renderTargets.DenoiserDisocclusionThresholdMix;
                    break;
                case nrd::ResourceType::TRANSIENT_POOL:
                    texture = m_TransientTextures[resource.indexInPool];
                    break;
                case nrd::ResourceType::PERMANENT_POOL:
                    texture = m_PermanentTextures[resource.indexInPool];
                    break;
                default:
                    assert(!"Unavailable resource type");
                    break;
                }

                assert(texture);

                nvrhi::TextureSubresourceSet subresources = nvrhi::AllSubresources;
                subresources.baseMipLevel = resource.mipOffset;
                subresources.numMipLevels = resource.mipNum;

                nvrhi::BindingSetItem setItem = nvrhi::BindingSetItem::None();
                setItem.resourceHandle = texture;
                setItem.slot = nrdDescriptorRange.baseRegisterIndex + descriptorOffset;
                setItem.subresources = subresources;
                setItem.type = (nrdDescriptorRange.descriptorType == nrd::DescriptorType::TEXTURE)
                    ? nvrhi::ResourceType::Texture_SRV
                    : nvrhi::ResourceType::Texture_UAV;

                setDesc.bindings.push_back(setItem);

                resourceIndex++;
            }
        }

        assert(resourceIndex == dispatchDesc.resourcesNum);

        const NrdPipeline& pipeline = m_Pipelines[dispatchDesc.pipelineIndex];

        nvrhi::BindingSetHandle bindingSet = m_BindingCache.GetOrCreateBindingSet(setDesc, pipeline.BindingLayout);

        nvrhi::ComputeState state;
        state.bindings = { bindingSet };
        state.pipeline = pipeline.Pipeline;
        commandList->setComputeState(state);

        commandList->dispatch(dispatchDesc.gridWidth, dispatchDesc.gridHeight);

        if (dispatchDesc.name)
        {
            commandList->endMarker();
        }
    }
}

#endif