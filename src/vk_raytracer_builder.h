#pragma once

#include <vk_types.h>


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

    void build_blas(const std::vector<BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags);
};