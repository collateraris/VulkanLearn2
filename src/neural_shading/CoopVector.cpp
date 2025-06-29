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
#include "CoopVector.h"
#include <algorithm>

using namespace rtxns;

namespace
{
/**
 * Bytes between a consecutive row or column (if row/column-major layout).
 * The stride is only used for row/column major layouts
 **/
size_t GetStride(const MatrixLayout layout, const uint32_t rows, const uint32_t cols, const size_t precision)
{
    size_t stride = 0;
    if (layout == MatrixLayout::RowMajor)
    {
        stride = cols * precision;
    }
    else if (layout == MatrixLayout::ColumnMajor)
    {
        stride = rows * precision;
    }
    return stride;
}
} // namespace

namespace
{

VkComponentTypeKHR GetVkComponentType(rtxns::Precision precision)
{
    return precision == rtxns::Precision::F16 ? VK_COMPONENT_TYPE_FLOAT16_NV : VK_COMPONENT_TYPE_FLOAT32_NV;
}

VkCooperativeVectorMatrixLayoutNV GetVkLayout(const MatrixLayout layout)
{
    switch (layout)
    {
    case MatrixLayout::RowMajor:
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV;
    case MatrixLayout::ColumnMajor:
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_COLUMN_MAJOR_NV;
    case MatrixLayout::InferencingOptimal:
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV;
    case MatrixLayout::TrainingOptimal:
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_TRAINING_OPTIMAL_NV;
    default:
        return VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_MAX_ENUM_NV;
    }
}

VkConvertCooperativeVectorMatrixInfoNV GetVkConvertLayerDesc(
    int rows, int columns, Precision precision, MatrixLayout srcLayout, MatrixLayout dstLayout, size_t srcSize, size_t* dstSize, uint64_t srcData = 0, uint64_t dstData = 0)
{
    VkConvertCooperativeVectorMatrixInfoNV info{};
    info.sType = VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV;
    info.pNext = nullptr;
    info.numRows = rows;
    info.numColumns = columns;
    info.srcComponentType = GetVkComponentType(precision);
    info.srcLayout = GetVkLayout(srcLayout);
    info.srcStride = GetStride(MatrixLayout::RowMajor, rows, columns, GetSize(precision));
    info.srcSize = srcSize;
    info.srcData.deviceAddress = srcData;
    info.dstComponentType = GetVkComponentType(precision);
    info.dstLayout = GetVkLayout(dstLayout);
    info.dstStride = GetStride(dstLayout, rows, columns, GetSize(precision));
    info.pDstSize = dstSize;
    info.dstData.deviceAddress = dstData;
    return info;
}

} // namespace

CoopVectorUtils_VK::CoopVectorUtils_VK(VulkanEngine* engine)
{
    _engine = engine;
}

size_t CoopVectorUtils_VK::QueryMatrixByteSize(const uint32_t rows, const uint32_t cols, const MatrixLayout layout, const Precision precision)
{
    assert(rows > 0 && rows <= 128 && "Number of rows must be 1..128.");
    assert(cols > 0 && cols <= 128 && "Number of columns must be 1..128.");

    size_t requiredSize = 0;

    VkConvertCooperativeVectorMatrixInfoNV info = GetVkConvertLayerDesc(rows, cols, precision, MatrixLayout::RowMajor, layout, 0, &requiredSize);

    VkResult res = vkConvertCooperativeVectorMatrixNV(_engine->_device, &info);
    assert(res == VK_SUCCESS && "Call to vkConvertCooperativeVectorMatrixNV failed");
    assert(requiredSize > 0 && "Expected matrix size to be larger than zero.");

    return requiredSize;
}

void CoopVectorUtils_VK::ConvertDeviceMatrixLayout(
    NetworkLayout const& srcLayout, NetworkLayout const& dstLayout, AllocatedBuffer& srcBuffer, uint64_t srcBufferOffset, AllocatedBuffer& dstBuffer, uint64_t dstBufferOffset) const
{
    VkBuffer vkSrcBuffer = srcBuffer._buffer;
    VkBuffer vkDstBuffer = dstBuffer._buffer;

    // Obtain the device addresses of the buffers for the conversion functions
    VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
    bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAddressInfo.buffer = vkSrcBuffer;
    VkDeviceAddress const srcBufferVA = vkGetBufferDeviceAddress(_engine->_device, &bufferDeviceAddressInfo);
    bufferDeviceAddressInfo.buffer = vkDstBuffer;
    VkDeviceAddress const dstBufferVA = vkGetBufferDeviceAddress(_engine->_device, &bufferDeviceAddressInfo);

    // Convert weights
    std::vector<VkConvertCooperativeVectorMatrixInfoNV> convertInfos(srcLayout.networkLayers.size());
    for (int i = 0; i < srcLayout.networkLayers.size(); i++)
    {
        // Weights
        size_t dstLayerSize = dstLayout.networkLayers[i].weightSize;
        convertInfos[i] =
            GetVkConvertLayerDesc(srcLayout.networkLayers[i].outputs, srcLayout.networkLayers[i].inputs, srcLayout.matrixPrecision, srcLayout.matrixLayout, dstLayout.matrixLayout,
                                  srcLayout.networkLayers[i].weightSize, &dstLayerSize, srcBufferVA + srcBufferOffset + srcLayout.networkLayers[i].weightOffset,
                                  dstBufferVA + dstBufferOffset + dstLayout.networkLayers[i].weightOffset);
    }
    _engine->immediate_submit([&](VkCommandBuffer cmd) {
        vkCmdConvertCooperativeVectorMatrixNV(cmd, (uint32_t)convertInfos.size(), convertInfos.data());

        // Copy the bias
        std::vector<VkBufferCopy> copyRegions(srcLayout.networkLayers.size());
        for (int i = 0; i < srcLayout.networkLayers.size(); i++)
        {
            copyRegions[i].srcOffset = srcBufferOffset + srcLayout.networkLayers[i].biasOffset;
            copyRegions[i].dstOffset = dstBufferOffset + dstLayout.networkLayers[i].biasOffset;
            copyRegions[i].size = srcLayout.networkLayers[i].biasSize;
        }
        vkCmdCopyBuffer(cmd, vkSrcBuffer, vkDstBuffer, (uint32_t)copyRegions.size(), copyRegions.data());
    });
}


