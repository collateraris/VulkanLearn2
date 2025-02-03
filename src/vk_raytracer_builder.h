#pragma once

#include <vk_types.h>
#if RAYTRACER_ON

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

    void build_tlas(VulkanEngine& engine, std::vector<VkAccelerationStructureInstanceKHR>& instances,
        VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        bool                                 update = false);

    static AccelerationStruct create_acceleration(VulkanEngine& engine, VkAccelerationStructureCreateInfoKHR& accel_);

    VkDeviceAddress get_blas_device_address(VkDevice _device, uint32_t blasId);

    VkAccelerationStructureKHR get_acceleration_structure() const;

    static AllocatedBuffer create_SBTBuffer(VulkanEngine* engine, uint32_t missCount, uint32_t hitCount, EPipelineType pipType,
        VkStridedDeviceAddressRegionKHR& rgenRegion,
        VkStridedDeviceAddressRegionKHR& missRegion,
        VkStridedDeviceAddressRegionKHR& hitRegion,
        VkStridedDeviceAddressRegionKHR& callRegion);

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

#endif