#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct VertexInputDescription {

	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};


struct Vertex {

	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();
};

struct Meshlet
{
	std::array<uint32_t, 64> vertices = {0};
	std::array<uint32_t, 126> indices = {0}; // up to 42 triangles
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;
};

struct Mesh {
    std::vector<Vertex> _vertices;
	std::vector<uint16_t> _indices;
#if MESHSHADER_ON
	std::vector<Meshlet> _meshlets;
	std::array<VkDescriptorSet, 2> meshletsSet{VK_NULL_HANDLE };
#endif
	std::array<AllocatedBuffer, 2> _vertexBuffer;
#if MESHSHADER_ON
	std::array<AllocatedBuffer, 2> _meshletsBuffer;
#else
	std::array<AllocatedBuffer, 2> _indicesBuffer;
#endif

	bool load_from_obj(const char* filename);
#if MESHSHADER_ON
	void buildMeshlets();
#endif
};
