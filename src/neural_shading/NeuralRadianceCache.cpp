#include "NeuralRadianceCache.h"

#include <vk_engine.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

void NeuralRadianceCache::init(VulkanEngine* engine)
{
	_engine = engine;

    VkPhysicalDeviceCooperativeVectorPropertiesNV cooperativeProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_PROPERTIES_NV };
    VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    properties.pNext = &cooperativeProperties;
    vkGetPhysicalDeviceProperties2(_engine->_chosenPhysicalDeviceGPU, &properties);
    if (!cooperativeProperties.cooperativeVectorTrainingFloat32Accumulation)
        throw std::runtime_error("NRC requires cooperative-vector FP32 training accumulation");

    m_networkUtils = std::make_shared<rtxns::NetworkUtilities>(_engine);
    m_neuralNetwork = std::make_unique<rtxns::HostNetwork>(m_networkUtils);
    if (!m_neuralNetwork->Initialise(m_netArch))
    {
        _engine->_logger.debug_log("Failed to create a network.");
        return;
    }

	create_mlp_buffers();
}

void NeuralRadianceCache::create_mlp_buffers()
{
    const auto& params = m_neuralNetwork->GetNetworkParams();

    // Get a device optimized layout
    m_deviceNetworkLayout = m_networkUtils->GetNewMatrixLayout(m_neuralNetwork->GetNetworkLayout(), rtxns::MatrixLayout::TrainingOptimal);
    m_deviceGradientLayout = m_deviceNetworkLayout;
    m_deviceGradientLayout.matrixPrecision = rtxns::Precision::F32;
    m_networkUtils->SetNetworkLayerSizes(m_deviceGradientLayout);

    for (int i = 0; i < NUM_TRANSITIONS; ++i)
    {
        m_weightOffsets[i / 4][i % 4] = m_deviceNetworkLayout.networkLayers[i].weightOffset;
        m_biasOffsets[i / 4][i % 4] = m_deviceNetworkLayout.networkLayers[i].biasOffset;
        m_gradientWeightOffsets[i / 4][i % 4] = m_deviceGradientLayout.networkLayers[i].weightOffset;
        m_gradientBiasOffsets[i / 4][i % 4] = m_deviceGradientLayout.networkLayers[i].biasOffset;
    }

    // Every optimizer buffer indexes the converted layout, including its padding.
    m_totalParameterCount = uint32_t(m_deviceNetworkLayout.networkSize / sizeof(uint16_t));
    m_batchSize = std::min(uint32_t(BATCH_SIZE), _engine->_renderExtent.width * _engine->_renderExtent.height);

    m_mlpHostBuffer = _engine->create_buffer_n_copy_data(params.size(), m_neuralNetwork->GetNetworkParams().data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    std::vector<uint16_t> zeroHalf(m_totalParameterCount, 0);
    m_mlpDeviceBuffer = _engine->create_buffer_n_copy_data(m_deviceNetworkLayout.networkSize, zeroHalf.data(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    // Convert to GPU optimized layout
    m_networkUtils->ConvertWeights(m_neuralNetwork->GetNetworkLayout(), m_deviceNetworkLayout, m_mlpHostBuffer, 0, m_mlpDeviceBuffer, 0);

    // The first optimizer step copies the converted FP16 values into the FP32
    // master buffer. Host row-major values cannot initialize this layout.
    std::vector<float> zeroFloat(m_totalParameterCount, 0.f);
    m_mlpParamsBuffer32 = _engine->create_buffer_n_copy_data(m_totalParameterCount * sizeof(float), zeroFloat.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const size_t gradientBufferSize = m_deviceGradientLayout.networkSize * NRC_GRADIENT_BUCKET_COUNT;
    std::vector<float> zeroGradients(gradientBufferSize / sizeof(float), 0.f);
    m_mlpGradientsBuffer = _engine->create_buffer_n_copy_data(gradientBufferSize, zeroGradients.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto gradientIndices = create_gradient_index_map();
    m_gradientIndexMapBuffer = _engine->create_buffer_n_copy_data(gradientIndices.size() * sizeof(uint32_t), gradientIndices.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_mlpMoments1Buffer = _engine->create_buffer_n_copy_data(m_totalParameterCount * sizeof(float), zeroFloat.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_mlpMoments2Buffer = _engine->create_buffer_n_copy_data(m_totalParameterCount * sizeof(float), zeroFloat.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

}

std::vector<uint32_t> NeuralRadianceCache::create_gradient_index_map()
{
    constexpr uint32_t invalid = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> mapping(m_totalParameterCount, invalid);
    for (size_t layerIndex = 0; layerIndex < m_deviceNetworkLayout.networkLayers.size(); ++layerIndex)
    {
        const auto& weights = m_deviceNetworkLayout.networkLayers[layerIndex];
        const auto& gradients = m_deviceGradientLayout.networkLayers[layerIndex];
        const uint32_t elementCount = weights.inputs * weights.outputs;
        // Tags are distinct NORMAL half bit patterns, not integer-to-half
        // conversions (which collide above 2048). The FP32 copy is exact.
        if (elementCount > 0x7800u)
            throw std::runtime_error("NRC matrix too large for exact layout tags");
        std::vector<uint16_t> tags16(elementCount);
        std::vector<float> tags32(elementCount);
        for (uint32_t i = 0; i < elementCount; ++i)
        {
            tags16[i] = uint16_t(0x0400u + i);
            tags32[i] = rtxns::float16ToFloat32(tags16[i]);
        }
        std::vector<uint16_t> converted16(weights.weightSize / sizeof(uint16_t), 0);
        std::vector<float> converted32(gradients.weightSize / sizeof(float), 0.f);
        auto convertTags = [&](const void* source, void* destination, VkComponentTypeKHR component,
                               size_t elementSize, size_t destinationSize) {
            VkConvertCooperativeVectorMatrixInfoNV info{ VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV };
            info.srcSize = elementCount * elementSize;
            info.srcData.hostAddress = source;
            info.pDstSize = &destinationSize;
            info.dstData.hostAddress = destination;
            info.srcComponentType = component;
            info.dstComponentType = component;
            info.numRows = weights.outputs;
            info.numColumns = weights.inputs;
            info.srcLayout = VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV;
            info.srcStride = weights.inputs * elementSize;
            info.dstLayout = VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV;
            if (vkConvertCooperativeVectorMatrixNV(_engine->_device, &info) != VK_SUCCESS)
                throw std::runtime_error("Failed to build NRC gradient layout mapping");
        };
        convertTags(tags16.data(), converted16.data(), VK_COMPONENT_TYPE_FLOAT16_NV, sizeof(uint16_t), weights.weightSize);
        convertTags(tags32.data(), converted32.data(), VK_COMPONENT_TYPE_FLOAT32_NV, sizeof(float), gradients.weightSize);
        std::vector<uint32_t> halfPositions(elementCount, invalid);
        std::vector<uint32_t> floatPositions(elementCount, invalid);
        for (uint32_t i = 0; i < converted16.size(); ++i)
        {
            uint32_t tag = uint32_t(converted16[i]) - 0x0400u;
            if (tag < elementCount)
            {
                if (halfPositions[tag] != invalid)
                    throw std::runtime_error("Ambiguous NRC FP16 matrix layout");
                halfPositions[tag] = i;
            }
        }
        for (uint32_t i = 0; i < converted32.size(); ++i)
        {
            uint32_t tag = uint32_t(rtxns::float32ToFloat16(converted32[i])) - 0x0400u;
            if (tag < elementCount)
            {
                if (floatPositions[tag] != invalid || converted32[i] != tags32[tag])
                    throw std::runtime_error("Ambiguous NRC FP32 gradient layout");
                floatPositions[tag] = i;
            }
        }
        for (uint32_t i = 0; i < elementCount; ++i)
        {
            if (halfPositions[i] == invalid || floatPositions[i] == invalid)
                throw std::runtime_error("Incomplete NRC gradient layout mapping");
            mapping[weights.weightOffset / sizeof(uint16_t) + halfPositions[i]] =
                gradients.weightOffset / sizeof(float) + floatPositions[i];
        }
        for (uint32_t i = 0; i < weights.outputs; ++i)
            mapping[weights.biasOffset / sizeof(uint16_t) + i] = gradients.biasOffset / sizeof(float) + i;
    }
    return mapping;
}
