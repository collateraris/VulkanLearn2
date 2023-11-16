#pragma once

#include <vk_types.h>
#include <vk_utils.h>

class VulkanEngine;

namespace vkutil {

	class DescriptorAllocator {
	public:

		struct PoolSizes {
			std::vector<std::pair<VkDescriptorType, float>> sizes =
			{
				{ VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
				{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
				{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
				{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
				{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
			};
		};

		void reset_pools();
		bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

		void init(VkDevice newDevice, VkDescriptorPoolCreateFlags _flags = 0, size_t _poolSize = 1000);

		void cleanup();

		VkDevice device;
	private:
		VkDescriptorPool createPool(VkDevice device, const DescriptorAllocator::PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags);
		VkDescriptorPool grab_pool();

		VkDescriptorPool currentPool{ VK_NULL_HANDLE };
		PoolSizes descriptorSizes;
		std::vector<VkDescriptorPool> usedPools;
		std::vector<VkDescriptorPool> freePools;
		VkDescriptorPoolCreateFlags flags = 0;
		size_t poolSize = 1000;
	};

	class DescriptorLayoutCache {
	public:
		void init(VkDevice newDevice);
		void cleanup();

		VkDescriptorSetLayout create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);

		struct DescriptorLayoutInfo {
			//good idea to turn this into a inlined array
			std::vector<VkDescriptorSetLayoutBinding> bindings;

			bool operator==(const DescriptorLayoutInfo& other) const;

			size_t hash() const;
		};



	private:

		struct DescriptorLayoutHash {

			std::size_t operator()(const DescriptorLayoutInfo& k) const {
				return k.hash();
			}
		};

		std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> layoutCache;
		VkDevice device;
	};

	class DescriptorBuilder {
	public:
		static DescriptorBuilder begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator);

		DescriptorBuilder& bind_buffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1);
		DescriptorBuilder& bind_image(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags,uint32_t descriptorCount = 1);
		DescriptorBuilder& bind_rt_as(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* accelInfo, VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t descriptorCount = 1);

		bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
		bool build(VkDescriptorSet& set);

		bool build_bindless(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
	private:

		std::vector<VkWriteDescriptorSet> writes;
		std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindingsMap;

		DescriptorLayoutCache* cache;
		DescriptorAllocator* alloc;
	};

	class BindlessParams
	{
		struct Range
		{
			uint32_t offset;
			uint32_t size;
			void* data;
		};

	public:
		BindlessParams(uint32_t minAlignment, VulkanEngine* e) : _minAlignment(minAlignment), _engine(e) {}

		template<class TData>
		uint32_t addRange(TData&& data) {
			// Copy data to heap and store void pointer
			// since we do not care about the type at
			// point
			size_t dataSize = sizeof(TData);
			auto* bytes = new TData;
			*bytes = data;

			// Add range
			uint32_t currentOffset = _lastOffset;
			_ranges.push_back({ currentOffset, dataSize, bytes });

			// Pad the data size to minimum alignment
			// and move the offset
			_lastOffset += vk_utils::padSizeToAlignment(dataSize, _minAlignment);
			return currentOffset;
		}

		void build(uint32_t binding, DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator, VmaAllocator& vmaallocator);

		inline VkDescriptorSet getDescriptorSet() { return _descriptorSet; }

		inline VkDescriptorSetLayout getDescriptorSetLayout() { return _layout; }

	private:
		uint32_t _minAlignment;
		uint32_t _lastOffset = 0;
		std::vector<Range> _ranges;

		VkDescriptorSetLayout _layout;
		VkDescriptorSet _descriptorSet;

		AllocatedBuffer _rangeBuffer;

		VulkanEngine* _engine = nullptr;
	};
}
