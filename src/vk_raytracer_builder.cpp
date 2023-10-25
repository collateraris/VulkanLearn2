#include <vk_raytracer_builder.h>

#include <vk_engine.h>

void VulkanRaytracerBuilder::build_blas(VulkanEngine& engine, const std::vector<VulkanRaytracerBuilder::BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags)
{
	uint32_t numBlas = input.size();
	VkDeviceSize asTotalSize{ 0 };     // Memory size of all allocated BLAS
	uint32_t     numCompactions{ 0 };   // Nb of BLAS requesting compaction
	VkDeviceSize maxScratchSize{ 0 };  // Largest scratch size

    // Preparing the information for the acceleration build commands.
    std::vector<VulkanRaytracerBuilder::BuildAccelerationStructure> buildAs(numBlas);
    for (uint32_t idx = 0; idx < numBlas; idx++)
    {
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
        buildAs[idx].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildAs[idx].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildAs[idx].buildInfo.flags = input[idx].flags | flags;
        buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(input[idx].asGeometry.size());
        buildAs[idx].buildInfo.pGeometries = input[idx].asGeometry.data();

        // Build range information
        buildAs[idx].rangeInfo = input[idx].asBuildOffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> maxPrimCount(input[idx].asBuildOffsetInfo.size());
        for (auto tt = 0; tt < input[idx].asBuildOffsetInfo.size(); tt++)
            maxPrimCount[tt] = input[idx].asBuildOffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
        vkGetAccelerationStructureBuildSizesKHR(engine._device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildAs[idx].buildInfo, maxPrimCount.data(), &buildAs[idx].sizeInfo);

        // Extra info
        asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
        numCompactions += hasFlag(buildAs[idx].buildInfo.flags, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
    }

    AllocatedBuffer scratchBuffer =
        engine.create_gpuonly_buffer_with_device_address(maxScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer._buffer };
    VkDeviceAddress           scratchAddress = vkGetBufferDeviceAddress(engine._device, &bufferInfo);

    // Batching creation/compaction of BLAS to allow staying in restricted amount of memory
    std::vector<uint32_t> indices;  // Indices of the BLAS to create
    VkDeviceSize          batchSize{ 0 };
    VkDeviceSize          batchLimit{ 256'000'000 };  // 256 MB

    for (uint32_t idx = 0; idx < numBlas; idx++)
    {
        indices.push_back(idx);
        batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (batchSize >= batchLimit || idx == numBlas - 1)
        {
            engine.immediate_submit([&](VkCommandBuffer cmd) {
                create_blas(engine, cmd, indices, buildAs, scratchAddress);
                });

            // Reset
            batchSize = 0;
            indices.clear();
        }
    }

    // Keeping all the created acceleration structures
    for (auto& b : buildAs)
    {
        _blas.emplace_back(b.as);
    }
}

//--------------------------------------------------------------------------------------------------
// Creating the bottom level acceleration structure for all indices of `buildAs` vector.
// The array of BuildAccelerationStructure was created in buildBlas and the vector of
// indices limits the number of BLAS to create at once. This limits the amount of
// memory needed when compacting the BLAS.
void VulkanRaytracerBuilder::create_blas(VulkanEngine& engine, VkCommandBuffer cmd, std::vector<uint32_t> indices, std::vector<VulkanRaytracerBuilder::BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress)
{
    uint32_t queryCnt{ 0 };

    for (const auto& idx : indices)
    {
        // Actual allocation of buffer and acceleration structure.
        VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;  // Will be used to allocate memory.
        buildAs[idx].as = create_acceleration(engine, createInfo);

        // BuildInfo #2 part
        buildAs[idx].buildInfo.dstAccelerationStructure = buildAs[idx].as.accel;  // Setting where the build lands
        buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress;  // All build are using the same scratch buffer

        // Building the bottom-level-acceleration-structure
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildAs[idx].buildInfo, &buildAs[idx].rangeInfo);

        // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
        // is finished before starting the next one.
        VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
}

AccelerationStruct VulkanRaytracerBuilder::create_acceleration(VulkanEngine& engine, VkAccelerationStructureCreateInfoKHR& accel_)
{
    AccelerationStruct resultAccel;
    // Allocating the buffer to hold the acceleration structure
    resultAccel.buffer = engine.create_gpuonly_buffer_with_device_address(accel_.size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    // Setting the buffer
    accel_.buffer = resultAccel.buffer._buffer;
    // Create the acceleration structure
    vkCreateAccelerationStructureKHR(engine._device, &accel_, nullptr, &resultAccel.accel);

    return resultAccel;
}
