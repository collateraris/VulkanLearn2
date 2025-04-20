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

void VulkanRaytracerBuilder::build_tlas(VulkanEngine& engine, std::vector<VkAccelerationStructureInstanceKHR>& instances, VkBuildAccelerationStructureFlagsKHR flags, bool update)
{
    uint32_t countInstance = static_cast<uint32_t>(instances.size());

    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
    AllocatedBuffer instancesBuffer = // Buffer of instances containing the matrices and BLAS ids
        engine.create_buffer_n_copy_data(countInstance * sizeof(VkAccelerationStructureInstanceKHR), instances.data(), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

    VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, instancesBuffer._buffer };
    VkDeviceAddress           instBufferAddr = vkGetBufferDeviceAddress(engine._device, &bufferInfo);

    // Wraps a device pointer to the above uploaded instances.
    VkAccelerationStructureGeometryInstancesDataKHR instancesVk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
    instancesVk.data.deviceAddress = instBufferAddr;

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    VkAccelerationStructureGeometryKHR topASGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topASGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;  // Avoid double hits;
    topASGeometry.geometry.instances = instancesVk;

    // Find sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.flags = flags;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &topASGeometry;
    buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(engine._device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
        &countInstance, &sizeInfo);

    // Create TLAS
    if (update == false)
    {
        VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        createInfo.size = sizeInfo.accelerationStructureSize;

        _tlas = create_acceleration(engine, createInfo);
    }

    {
        AllocatedBuffer scratchBuffer = engine.create_gpuonly_buffer_with_device_address(sizeInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

        VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, scratchBuffer._buffer };
        VkDeviceAddress           scratchAddress = vkGetBufferDeviceAddress(engine._device, &bufferInfo);

        // Update build information
        buildInfo.srcAccelerationStructure = update ? _tlas.accel : VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = _tlas.accel;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        // Build Offsets info: n instances
        VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ countInstance, 0, 0, 0 };
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

        // Build the TLAS
        engine.immediate_submit([&](VkCommandBuffer cmd) {
            vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildOffsetInfo);
            });
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
        barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
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

VkDeviceAddress VulkanRaytracerBuilder::get_blas_device_address(VkDevice _device, uint32_t blasId)
{
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addressInfo.accelerationStructure = _blas[blasId].accel;
    return vkGetAccelerationStructureDeviceAddressKHR(_device, &addressInfo);
}

VkAccelerationStructureKHR VulkanRaytracerBuilder::get_acceleration_structure() const
{
    return _tlas.accel;
}

AllocatedBuffer VulkanRaytracerBuilder::create_SBTBuffer(VulkanEngine* engine, uint32_t missCount, uint32_t hitCount, EPipelineType pipType,
    VkStridedDeviceAddressRegionKHR& rgenRegion,
    VkStridedDeviceAddressRegionKHR& missRegion,
    VkStridedDeviceAddressRegionKHR& hitRegion,
    VkStridedDeviceAddressRegionKHR& callRegion)
{
    auto     handleCount = 1 + missCount + hitCount;
    uint32_t handleSize = engine->_rtProperties.shaderGroupHandleSize;

    // The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
    uint32_t handleSizeAligned = vkutil::align_up(handleSize, engine->_rtProperties.shaderGroupHandleAlignment);

    rgenRegion.stride = vkutil::align_up(handleSizeAligned, engine->_rtProperties.shaderGroupBaseAlignment);
    rgenRegion.size = rgenRegion.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
    missRegion.stride = handleSizeAligned;
    missRegion.size = vkutil::align_up(missCount * handleSizeAligned, engine->_rtProperties.shaderGroupBaseAlignment);
    hitRegion.stride = handleSizeAligned;
    hitRegion.size = vkutil::align_up(hitCount * handleSizeAligned, engine->_rtProperties.shaderGroupBaseAlignment);

    // Get the shader group handles
    uint32_t             dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);
    auto result = vkGetRayTracingShaderGroupHandlesKHR(engine->_device, engine->_renderPipelineManager.get_pipeline(pipType), 0, handleCount, dataSize, handles.data());

    // Allocate a buffer for storing the SBT.
    VkDeviceSize sbtSize = rgenRegion.size + missRegion.size + hitRegion.size + callRegion.size;
    AllocatedBuffer rtSBTBuffer = engine->create_staging_buffer(sbtSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Find the SBT addresses of each group
    VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, rtSBTBuffer._buffer };
    VkDeviceAddress           sbtAddress = vkGetBufferDeviceAddress(engine->_device, &info);
    rgenRegion.deviceAddress = sbtAddress;
    missRegion.deviceAddress = sbtAddress + rgenRegion.size;
    hitRegion.deviceAddress = sbtAddress + rgenRegion.size + missRegion.size;

    // Helper to retrieve the handle data
    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

    engine->map_buffer(engine->_allocator, rtSBTBuffer._allocation, [&](void*& data) {
        uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(data);
        uint8_t* pData = pSBTBuffer;
        uint32_t handleIdx{ 0 };
        // Raygen
        memcpy(pData, getHandle(handleIdx++), handleSize);
        // Miss
        pData = pSBTBuffer + rgenRegion.size;
        for (uint32_t i = 0; i < missCount; i++)
        {
            memcpy(pData, getHandle(handleIdx++), handleSize);
            pData += missRegion.stride;
        }
        // Hit
        pData = pSBTBuffer + rgenRegion.size + missRegion.size;
        for (uint32_t i = 0; i < hitCount; i++)
        {
            memcpy(pData, getHandle(handleIdx++), handleSize);
            pData += hitRegion.stride;
        }

    });

    return rtSBTBuffer;
}
