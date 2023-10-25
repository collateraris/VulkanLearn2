#pragma once

#include <vk_types.h>

class VulkanEngine;

class VulkanRaytracerBuilder
{
public:
    // Inputs used to build Bottom-level acceleration structure.
    // You manage the lifetime of the buffer(s) referenced by the VkAccelerationStructureGeometryKHRs within.
    // In particular, you must make sure they are still valid and not being modified when the BLAS is built or updated.
    struct BlasInput
    {
        // Data used to build acceleration structure geometry
        std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
        VkBuildAccelerationStructureFlagsKHR                  flags{ 0 };
    };

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
        AccelerationStruct                                  as;  // result acceleration structure
        AccelerationStruct                                  cleanupAS;
    };

    void build_blas(VulkanEngine& engine, const std::vector<VulkanRaytracerBuilder::BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags);

    static AccelerationStruct create_acceleration(VulkanEngine& engine, VkAccelerationStructureCreateInfoKHR& accel_);

protected:

    std::vector<AccelerationStruct> _blas;  // Bottom-level acceleration structure
    AccelerationStruct              _tlas;  // Top-level acceleration structure

    bool hasFlag(VkFlags item, VkFlags flag) { return (item & flag) == flag; };

    void create_blas(VulkanEngine& engine,
        VkCommandBuffer             cmd,
        std::vector<uint32_t>                    indices,
        std::vector<VulkanRaytracerBuilder::BuildAccelerationStructure>& buildAs,
        VkDeviceAddress                          scratchAddress);
};