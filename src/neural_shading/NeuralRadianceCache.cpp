#include "NeuralRadianceCache.h"

#include <vk_engine.h>

void NeuralRadianceCache::init(VulkanEngine* engine)
{
	_engine = engine;

    m_networkUtils = std::make_shared<rtxns::NetworkUtilities>(_engine);
    m_neuralNetwork = std::make_unique<rtxns::HostNetwork>(m_networkUtils);
    if (!m_neuralNetwork->Initialise(m_netArch))
    {
        _engine->_logger.debug_log("Failed to create a network.");
        return;
    }

    m_trainingConstantBuffer = _engine->create_cpu_to_gpu_buffer(sizeof(TrainingConstantBufferEntry), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	create_mlp_buffers();
}

void NeuralRadianceCache::create_mlp_buffers()
{
    const auto& params = m_neuralNetwork->GetNetworkParams();

    for (int i = 0; i < NUM_TRANSITIONS; ++i)
    {
        m_weightOffsets[i / 4][i % 4] = m_neuralNetwork->GetNetworkLayout().networkLayers[i].weightOffset;
        m_biasOffsets[i / 4][i % 4] = m_neuralNetwork->GetNetworkLayout().networkLayers[i].biasOffset;
    }

    // Get a device optimized layout
    m_deviceNetworkLayout = m_networkUtils->GetNewMatrixLayout(m_neuralNetwork->GetNetworkLayout(), rtxns::MatrixLayout::TrainingOptimal);

    m_totalParameterCount = uint32_t(params.size() / sizeof(uint16_t));
    m_batchSize = BATCH_SIZE;

    m_mlpHostBuffer = _engine->create_buffer_n_copy_data(params.size(), m_neuralNetwork->GetNetworkParams().data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    m_mlpDeviceBuffer = _engine->create_gpuonly_buffer(m_deviceNetworkLayout.networkSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    // Convert to GPU optimized layout
    m_networkUtils->ConvertWeights(m_neuralNetwork->GetNetworkLayout(), m_deviceNetworkLayout, m_mlpHostBuffer, 0, m_mlpDeviceBuffer, 0);

    std::vector<float> fbuf(m_totalParameterCount);
    std::transform((uint16_t*)params.data(), ((uint16_t*)params.data()) + m_totalParameterCount, fbuf.begin(), [](auto v) { return rtxns::float16ToFloat32(v); });
    m_mlpParamsBuffer32 = _engine->create_buffer_n_copy_data(m_totalParameterCount * sizeof(float), fbuf.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // Round up to nearest multiple of 4
    size_t mod = m_totalParameterCount % 4;
    std::vector<uint16_t> fbuf2(m_totalParameterCount + mod, 0);
    m_mlpGradientsBuffer = _engine->create_buffer_n_copy_data(((m_totalParameterCount * sizeof(uint16_t) + 3) & ~3), fbuf2.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    std::vector<float> fbuf3(m_totalParameterCount, 0);
    m_mlpMoments1Buffer = _engine->create_buffer_n_copy_data(m_totalParameterCount * sizeof(float), fbuf3.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_mlpMoments2Buffer = _engine->create_buffer_n_copy_data(m_totalParameterCount * sizeof(float), fbuf3.data(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

}
