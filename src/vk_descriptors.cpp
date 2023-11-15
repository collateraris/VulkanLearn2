#include <vk_descriptors.h>
#include <vk_engine.h>

void vkutil::DescriptorAllocator::reset_pools()
{
	//reset all used pools and add them to the free pools
	for (auto p : usedPools) {
		vkResetDescriptorPool(device, p, 0);
		freePools.push_back(p);
	}

	//clear the used pools, since we've put them all in the free pools
	usedPools.clear();

	//reset the current pool handle back to null
	currentPool = VK_NULL_HANDLE;
}

bool vkutil::DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
{
	//initialize the currentPool handle if it's null
	if (currentPool == VK_NULL_HANDLE) {

		currentPool = grab_pool();
		usedPools.push_back(currentPool);
	}

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.pSetLayouts = &layout;
	allocInfo.descriptorPool = currentPool;
	allocInfo.descriptorSetCount = 1;

	//try to allocate the descriptor set
	VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
	bool needReallocate = false;

	switch (allocResult) {
	case VK_SUCCESS:
		//all good, return
		return true;
	case VK_ERROR_FRAGMENTED_POOL:
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		//reallocate pool
		needReallocate = true;
		break;
	default:
		//unrecoverable error
		return false;
	}

	if (needReallocate) {
		//allocate a new pool and retry
		currentPool = grab_pool();
		usedPools.push_back(currentPool);

		allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

		//if it still fails then we have big issues
		if (allocResult == VK_SUCCESS) {
			return true;
		}
	}

	return false;
}

void vkutil::DescriptorAllocator::init(VkDevice newDevice, VkDescriptorPoolCreateFlags _flags/* = 0*/, size_t _poolSize/* = 1000*/)
{
	device = newDevice;
	flags = _flags;
	poolSize = _poolSize;
}

void vkutil::DescriptorAllocator::cleanup()
{
	//delete every pool held
	for (auto p : freePools)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	for (auto p : usedPools)
	{
		vkDestroyDescriptorPool(device, p, nullptr);
	}
}

VkDescriptorPool vkutil::DescriptorAllocator::createPool(VkDevice device, const DescriptorAllocator::PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags)
{
	std::vector<VkDescriptorPoolSize> sizes;
	sizes.reserve(poolSizes.sizes.size());
	for (auto sz : poolSizes.sizes) {
		sizes.push_back({ sz.first, uint32_t(sz.second * count) });
	}
	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = flags;
	pool_info.maxSets = count;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool);

	return descriptorPool;
}

VkDescriptorPool vkutil::DescriptorAllocator::grab_pool()
{
	//there are reusable pools availible
	if (freePools.size() > 0)
	{
		//grab pool from the back of the vector and remove it from there.
		VkDescriptorPool pool = freePools.back();
		freePools.pop_back();
		return pool;
	}
	else
	{
		//no pools availible, so create a new one
		return createPool(device, descriptorSizes, poolSize, flags);
	}
}

void vkutil::DescriptorLayoutCache::init(VkDevice newDevice)
{
	device = newDevice;
}

void vkutil::DescriptorLayoutCache::cleanup()
{
	//delete every descriptor layout held
	for (auto pair : layoutCache) {
		vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
	}
}

VkDescriptorSetLayout vkutil::DescriptorLayoutCache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info)
{
	DescriptorLayoutInfo layoutinfo;
	layoutinfo.bindings.reserve(info->bindingCount);
	bool isSorted = true;
	int lastBinding = -1;

	//copy from the direct info struct into our own one
	for (int i = 0; i < info->bindingCount; i++) {
		layoutinfo.bindings.push_back(info->pBindings[i]);

		//check that the bindings are in strict increasing order
		if (info->pBindings[i].binding > lastBinding) {
			lastBinding = info->pBindings[i].binding;
		}
		else {
			isSorted = false;
		}
	}
	//sort the bindings if they aren't in order
	if (!isSorted) {
		std::sort(layoutinfo.bindings.begin(), layoutinfo.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
			return a.binding < b.binding;
			});
	}

	//try to grab from cache
	auto it = layoutCache.find(layoutinfo);
	if (it != layoutCache.end()) {
		return (*it).second;
	}
	else {
		//create a new one (not found)
		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout(device, info, nullptr, &layout);

		//add to cache
		layoutCache[layoutinfo] = layout;
		return layout;
	}
}

bool vkutil::DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
	if (other.bindings.size() != bindings.size()) {
		return false;
	}
	else {
		//compare each of the bindings is the same. Bindings are sorted so they will match
		for (int i = 0; i < bindings.size(); i++) {
			if (other.bindings[i].binding != bindings[i].binding) {
				return false;
			}
			if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
				return false;
			}
			if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
				return false;
			}
			if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
				return false;
			}
		}
		return true;
	}
}

size_t vkutil::DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
{
	using std::size_t;
	using std::hash;

	size_t result = hash<size_t>()(bindings.size());

	for (const VkDescriptorSetLayoutBinding& b : bindings)
	{
		//pack the binding data into a single int64. Not fully correct but it's ok
		size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;

		//shuffle the packed binding data and xor it with the main hash
		result ^= hash<size_t>()(binding_hash);
	}

	return result;
}

vkutil::DescriptorBuilder vkutil::DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator)
{
	DescriptorBuilder builder;

	builder.cache = layoutCache;
	builder.alloc = allocator;
	return builder;
}

vkutil::DescriptorBuilder& vkutil::DescriptorBuilder::bind_buffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t  dstArrayElement/* = 0*/, uint32_t descriptorCount/* = 1*/)
{
	VkDescriptorSetLayoutBinding& newBinding = bindingsMap[binding];

	newBinding.descriptorCount = descriptorCount;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	//create the descriptor write
	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pBufferInfo = bufferInfo;
	newWrite.dstBinding = binding;
	newWrite.dstArrayElement = dstArrayElement;

	writes.push_back(newWrite);
	return *this;
}

vkutil::DescriptorBuilder& vkutil::DescriptorBuilder::bind_image(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t  dstArrayElement/* = 0*/, uint32_t descriptorCount/* = 1*/)
{
	VkDescriptorSetLayoutBinding& newBinding = bindingsMap[binding];

	newBinding.descriptorCount = descriptorCount;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pImageInfo = imageInfo;
	newWrite.dstBinding = binding;
	newWrite.dstArrayElement = dstArrayElement;

	writes.push_back(newWrite);
	return *this;
}

vkutil::DescriptorBuilder& vkutil::DescriptorBuilder::bind_rt_as(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* accelInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t  dstArrayElement/* = 0*/, uint32_t descriptorCount/* = 1*/)
{
	VkDescriptorSetLayoutBinding& newBinding = bindingsMap[binding];

	newBinding.descriptorCount = descriptorCount;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = accelInfo;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.dstBinding = binding;
	newWrite.dstArrayElement = dstArrayElement;

	writes.push_back(newWrite);
	return *this;
}

bool vkutil::DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings{};
	for (auto& [bindPoint, setLayoutBinding] : bindingsMap)
		bindings.push_back(setLayoutBinding);

	layoutInfo.pBindings = bindings.data();
	layoutInfo.bindingCount = bindings.size();

	layout = cache->create_descriptor_layout(&layoutInfo);

	//allocate descriptor
	bool success = alloc->allocate(&set, layout);
	if (!success) { return false; };

	//write descriptor
	for (VkWriteDescriptorSet& w : writes) {
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(alloc->device, writes.size(), writes.data(), 0, nullptr);

	return true;
}

bool vkutil::DescriptorBuilder::build(VkDescriptorSet& set)
{
	VkDescriptorSetLayout layout;
	return build(set, layout);
}

bool vkutil::DescriptorBuilder::build_bindless(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings{};
	for (auto& [bindPoint, setLayoutBinding] : bindingsMap)
		bindings.push_back(setLayoutBinding);

	std::vector<VkDescriptorBindingFlags> flags(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.pNext = nullptr;
	bindingFlags.pBindingFlags = flags.data();
	bindingFlags.bindingCount = flags.size();

	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.pNext = &bindingFlags;

	layoutInfo.pBindings = bindings.data();
	layoutInfo.bindingCount = bindings.size();

	layout = cache->create_descriptor_layout(&layoutInfo);

	//allocate descriptor
	bool success = alloc->allocate(&set, layout);
	if (!success) { return false; };

	//write descriptor
	for (VkWriteDescriptorSet& w : writes) {
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(alloc->device, writes.size(), writes.data(), 0, nullptr);

	return true;
}

void vkutil::BindlessParams::build(uint32_t binding, DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator, VmaAllocator& vmaallocator)
{
	_rangeBuffer = _engine->create_cpu_to_gpu_buffer(_lastOffset, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	// Copy ranges to buffer
	_engine->map_buffer(vmaallocator, _rangeBuffer._allocation, [&](void*& data) {
			uint8_t* _data = (uint8_t*)data;
			for (const auto& range : _ranges) {
				memcpy(_data + range.offset, range.data, range.size);
			}
		});

	// Get maximum size of a single range
	uint32_t maxRangeSize = 0;
	for (auto& range : _ranges) {
		maxRangeSize = std::max(range.size, maxRangeSize);
	}

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = _rangeBuffer._buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = _engine->padSizeToMinUniformBufferOffsetAlignment(maxRangeSize);

	vkutil::DescriptorBuilder::begin(layoutCache,allocator)
		.bind_buffer(binding, &bufferInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_ALL)
		.build(_descriptorSet, _layout);
}
