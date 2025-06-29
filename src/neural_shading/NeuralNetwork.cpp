/*
 * Copyright (c) 2015 - 2025, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <vk_engine.h>
#include <fstream>
#include <sstream>
#include <random>
#include "NeuralNetwork.h"

using namespace rtxns;

namespace
{
/// Helper to align an integer value to a given alignment.
template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type align_to(T alignment, T value)
{
    return ((value + alignment - T(1)) / alignment) * alignment;
}

const uint32_t HEADER_VERSION = 0xA1C0DE01;
const uint32_t MAX_SUPPORTED_LAYERS = 8;

struct NetworkFileHeader
{
    uint32_t version = HEADER_VERSION;
    NetworkArchitecture netArch;
    NetworkLayer layers[MAX_SUPPORTED_LAYERS];
    MatrixLayout layout;
    size_t dataSize;
};

} // namespace

NetworkUtilities::NetworkUtilities(VulkanEngine* engine)
{
    _engine = engine;
    m_coopVecUtils = std::make_unique<CoopVectorUtils_VK>(_engine);
    assert(m_coopVecUtils && "Failed to create CoopVec Utility");
}

bool rtxns::NetworkUtilities::ValidateNetworkArchitecture(NetworkArchitecture const& netArch)
{
    if (netArch.numHiddenLayers + 1 > MAX_SUPPORTED_LAYERS)
    {
        //log::error("Too many layers - %d > %d", netArch.numHiddenLayers + 1, MAX_SUPPORTED_LAYERS);
        return false;
    }

    if (netArch.inputNeurons * netArch.outputNeurons * netArch.hiddenNeurons == 0)
    {
        //log::error("Neuron counts must all be positive - (%d, %d, %d)", netArch.inputNeurons, netArch.outputNeurons, netArch.hiddenNeurons);
        return false;
    }

    // Only Float16 weights are supported in the SDK
    if (netArch.weightPrecision != Precision::F16)
    {
        //log::error("Weight precision not supported - must be f16.");
        return false;
    }

    if (netArch.biasPrecision != Precision::F16)
    {
        //log::error("Bias precision not supported - must be f16.");
        return false;
    }

    return true;
}

// Create host side network layout
rtxns::NetworkLayout rtxns::NetworkUtilities::CreateHostNetworkLayout(NetworkArchitecture const& netArch)
{
    NetworkLayout layout;
    layout.matrixPrecision = netArch.weightPrecision;
    layout.matrixLayout = MatrixLayout::RowMajor; // Host side matrix layout

    const uint32_t numLayers = netArch.numHiddenLayers + 1; // hidden layers + input
    size_t offset = 0;

    layout.networkLayers.clear();

    // Create a network layout from the provides architecture
    for (uint32_t i = 0; i < numLayers; i++)
    {
        uint32_t inputs = (i == 0) ? netArch.inputNeurons : netArch.hiddenNeurons;
        uint32_t outputs = (i == numLayers - 1) ? netArch.outputNeurons : netArch.hiddenNeurons;

        NetworkLayer layer = {};
        layer.inputs = inputs;
        layer.outputs = outputs;

        layout.networkLayers.push_back(layer);
    }

    SetNetworkLayerSizes(layout);

    return layout;
}

// Set the weight and bias size / offsets for each layer in the network.
void NetworkUtilities::SetNetworkLayerSizes(NetworkLayout& layout)
{
    size_t offset = 0;
    // Calculate size and offsets for the new layout
    for (int i = 0; i < layout.networkLayers.size(); i++)
    {
        NetworkLayer& layer = layout.networkLayers[i];
        layer.weightSize = m_coopVecUtils->QueryMatrixByteSize(layer.outputs, layer.inputs, layout.matrixLayout, layout.matrixPrecision);
        layer.biasSize = layer.outputs * GetSize(layout.matrixPrecision);

        offset = align_to(m_coopVecUtils->GetMatrixAlignment(), offset);
        layer.weightOffset = (uint32_t)offset;
        offset += layer.weightSize;

        offset = align_to(m_coopVecUtils->GetVectorAlignment(), offset);
        layer.biasOffset = (uint32_t)offset;
        offset += layer.biasSize;
    }
    offset = align_to(m_coopVecUtils->GetMatrixAlignment(), offset);
    layout.networkSize = offset;
}

// Returns a updated network layout where the weights and bias size / offsets have been update
// for the new matrix layout..
// Can be device optimal matrix layout
rtxns::NetworkLayout NetworkUtilities::GetNewMatrixLayout(NetworkLayout const& srcLayout, MatrixLayout newMatrixLayout)
{
    NetworkLayout newLayout = srcLayout;

    // Check the new matrix layout does not match the current one
    if (newMatrixLayout == srcLayout.matrixLayout)
    {
        return newLayout;
    }
    newLayout.matrixLayout = newMatrixLayout;
    SetNetworkLayerSizes(newLayout);
    return newLayout;
}

// Converts weights and bias buffers from src layout to the dst layout.
// Both buffers must be device side.
// Both networks must be of the same network layout, only differing in MatrixLayout
void NetworkUtilities::ConvertWeights(NetworkLayout const& srcLayout,
                NetworkLayout const& dstLayout,
                AllocatedBuffer& srcBuffer,
                uint64_t srcBufferOffset,
                AllocatedBuffer& dstBuffer,
                uint64_t dstBufferOffset)
{
    // Set required barriers
    //commandList->setBufferState(srcBuffer, nvrhi::ResourceStates::ShaderResource);
    //commandList->setBufferState(dstBuffer, nvrhi::ResourceStates::UnorderedAccess);
    //commandList->commitBarriers();

    // Convert the matrix parameters on the device
    m_coopVecUtils->ConvertDeviceMatrixLayout(srcLayout, dstLayout, srcBuffer, 0, dstBuffer, 0);
}

HostNetwork::HostNetwork(std::shared_ptr<NetworkUtilities> networkUtils) : m_networkUtils(networkUtils)
{
    assert(m_networkUtils && "Network Utilities not present");
}

// Create host side network with initial values.
bool rtxns::HostNetwork::Initialise(const NetworkArchitecture& netArch)
{
    m_networkArchitecture = netArch;
    if (!m_networkUtils->ValidateNetworkArchitecture(m_networkArchitecture))
    {
        //log::error("CreateTrainingNetwork: Failed to validate network.");
        return false;
    }

    // Compute size and offset of each weight matrix and bias vector.
    // These are placed after each other in memory with padding to fulfill the alignment requirements.
    m_networkLayout = m_networkUtils->CreateHostNetworkLayout(m_networkArchitecture);

    // Initialize the weight and bias
    m_networkParams.clear();
    m_networkParams.resize(m_networkLayout.networkSize, 0);

    static std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0, 1.0);

    for (uint32_t i = 0; i < m_networkLayout.networkLayers.size(); i++)
    {
        const auto& layer = m_networkLayout.networkLayers[i];
        std::vector<uint16_t> weights;
        weights.resize(size_t(layer.inputs * layer.outputs), 0);
        std::generate(weights.begin(), weights.end(), [&, k = sqrt(6.f / (layer.inputs + layer.outputs))]() { return rtxns::float32ToFloat16(dist(gen) * k); });
        std::memcpy(m_networkParams.data() + layer.weightOffset, weights.data(), layer.weightSize);

        std::vector<uint16_t> bias(layer.outputs);
        std::generate(bias.begin(), bias.end(), [&, k = sqrt(6.f / bias.size())]() { return rtxns::float32ToFloat16(dist(gen) * k); });
        std::memcpy(m_networkParams.data() + layer.biasOffset, bias.data(), layer.biasSize);
    }
    return true;
}

