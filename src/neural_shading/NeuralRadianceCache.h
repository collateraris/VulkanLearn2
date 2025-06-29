#pragma once
#include <vk_types.h>
#include "NetworkConfig.h"
#include "NeuralNetwork.h"

class VulkanEngine;

class NeuralRadianceCache
{
public:
	NeuralRadianceCache() {};
	~NeuralRadianceCache() {};

    void init(VulkanEngine* engine);

    glm::uvec4 m_weightOffsets[NUM_TRANSITIONS_ALIGN4];
    glm::uvec4 m_biasOffsets[NUM_TRANSITIONS_ALIGN4];

    AllocatedBuffer m_trainingConstantBuffer;
    AllocatedBuffer m_mlpHostBuffer;
    AllocatedBuffer m_mlpDeviceBuffer;
    AllocatedBuffer m_mlpParamsBuffer32;
    AllocatedBuffer m_mlpGradientsBuffer;
    AllocatedBuffer m_mlpMoments1Buffer;
    AllocatedBuffer m_mlpMoments2Buffer;

    uint32_t m_totalParameterCount = 0;
    uint32_t m_batchSize = BATCH_SIZE;
    uint32_t m_currentOptimizationStep = 0;
    float m_learningRate = LEARNING_RATE;

    std::shared_ptr<rtxns::NetworkUtilities> m_networkUtils;
    std::unique_ptr<rtxns::HostNetwork> m_neuralNetwork;
    rtxns::NetworkLayout m_deviceNetworkLayout;

    rtxns::NetworkArchitecture m_netArch = {
        .numHiddenLayers = NUM_HIDDEN_LAYERS,
        .inputNeurons = INPUT_NEURONS,
        .hiddenNeurons = HIDDEN_NEURONS,
        .outputNeurons = OUTPUT_NEURONS,
        .weightPrecision = rtxns::Precision::F16,
        .biasPrecision = rtxns::Precision::F16,
    };

private:

    void create_mlp_buffers();

    VulkanEngine* _engine = nullptr;
};