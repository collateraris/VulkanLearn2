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
#include <vector>


#include "Float16.h"
#include "NeuralNetworkTypes.h"

class VulkanEngine;

namespace rtxns
{

class ICoopVectorUtils
{
public:
    size_t GetMatrixAlignment()
    {
        return s_matrixAlignment;
    }
    size_t GetVectorAlignment()
    {
        return s_vectorAlignment;
    }

    /**
     * Query the size of a matrix in bytes.
     * @return Size of matrix in bytes.
     */
    virtual size_t QueryMatrixByteSize(const uint32_t rows, const uint32_t cols, const MatrixLayout layout, const Precision precision = Precision::F16) = 0;

    /**
     * Convert matrix on the device between any layouts.
     * The Precision must currently be the same.
     * @return Size of matrix in bytes.
     */
    virtual void ConvertDeviceMatrixLayout(NetworkLayout const& srcLayout,
                                           NetworkLayout const& dstLayout,
                                           AllocatedBuffer& srcBuffer,
                                           uint64_t srcBufferOffset,
                                           AllocatedBuffer& dstBuffer,
                                           uint64_t dstBufferOffset) const = 0;

protected:
    static const size_t s_matrixAlignment = 64; ///< Minimum byte alignment according to spec.
    static const size_t s_vectorAlignment = 16; ///< Minimum byte alignment according to spec.
};

class CoopVectorUtils_VK : public ICoopVectorUtils
{
public:
    CoopVectorUtils_VK(VulkanEngine* engine);

    /**
     * Query the size of a matrix in bytes.
     * @return Size of matrix in bytes.
     */
    size_t QueryMatrixByteSize(const uint32_t rows, const uint32_t cols, const MatrixLayout layout, const Precision precision = Precision::F16);

    /**
     * Convert matrix on the device between any layouts.
     * The Precision must currently be the same.
     * @return Size of matrix in bytes.
     */
    void ConvertDeviceMatrixLayout(
        NetworkLayout const& srcLayout, NetworkLayout const& dstLayout, AllocatedBuffer& srcBuffer, uint64_t srcBufferOffset, AllocatedBuffer& dstBuffer, uint64_t dstBufferOffset) const;

private:
    VulkanEngine* _engine = nullptr;
};

} // namespace rtxns