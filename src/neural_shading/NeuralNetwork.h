/*
 * Copyright (c) 2015 - 2025, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

#include <vk_types.h>
#include "CoopVector.h"
#include <vector>
#include <filesystem>

#include "NeuralNetworkTypes.h"

class VulkanEngine;

namespace rtxns
{

class NetworkUtilities
{
public:
    NetworkUtilities(VulkanEngine* engine);
    ~NetworkUtilities()
    {
    }

    bool ValidateNetworkArchitecture(NetworkArchitecture const& netArch);

    // Create host side network layout.
    NetworkLayout CreateHostNetworkLayout(NetworkArchitecture const& netArch);

    // Set the weights and bias size / offsets for each layer in the network.
    void SetNetworkLayerSizes(NetworkLayout& layout);

    // Returns a updated network layout where the weights and bias size / offsets have been update
    // for the new matrix layout
    // Can be device optimal matrix layout
    NetworkLayout GetNewMatrixLayout(NetworkLayout const& srcLayout, MatrixLayout newMatrixLayout);

    // Converts weights and bias buffers from src layout to the dst layout.
    // Both buffers must be device side.
    // Both networks must be of the same network layout, only differing in MatrixLayout
    void ConvertWeights(NetworkLayout const& srcLayout,
                        NetworkLayout const& dstLayout,
                        AllocatedBuffer& srcBuffer,
                        uint64_t srcBufferOffset,
                        AllocatedBuffer& dstBuffer,
                        uint64_t dstBufferOffset);

private:
    std::unique_ptr<ICoopVectorUtils> m_coopVecUtils;
    VulkanEngine* _engine = nullptr;
};

// Represent a host side neural network.
// Stores the network layout and parameters.
// Functionality to initialize a network to starting values or load from file.
// Also write parameters back to file
class HostNetwork
{
public:
    HostNetwork(std::shared_ptr<NetworkUtilities> networkUtils);
    ~HostNetwork(){};

    // Create host side network from provided architecture with initial values.
    bool Initialise(const NetworkArchitecture& netArch);

    const NetworkArchitecture& GetNetworkArchitecture() const
    {
        return m_networkArchitecture;
    }

    const std::vector<uint8_t>& GetNetworkParams() const
    {
        return m_networkParams;
    }

    const NetworkLayout& GetNetworkLayout() const
    {
        return m_networkLayout;
    }

private:
    std::shared_ptr<NetworkUtilities> m_networkUtils;
    NetworkArchitecture m_networkArchitecture;
    std::vector<uint8_t> m_networkParams;
    NetworkLayout m_networkLayout;
};
}; // namespace rtxns