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
	glm::vec3 color;
	glm::vec2 uv;

	static VertexInputDescription get_vertex_description();
};

struct Meshlet
{
	uint32_t vertices[64];
	uint8_t indices[126];
	uint8_t indexCount;
	uint8_t vertexCount;
};

struct Mesh {
    std::vector<Vertex> _vertices;
	std::vector<uint16_t> _indices;
	std::vector<Meshlet> _meshlets;

    AllocatedBuffer _vertexBuffer;
	AllocatedBuffer _indexBuffer;
	AllocatedBuffer _meshletsBuffer;

	void build_meshlets();

	bool load_from_obj(const char* filename);
};
