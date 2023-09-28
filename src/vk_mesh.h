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

struct Vertex_MS
{
	float vx, vy, vz, vw;
	uint8_t nx, ny, nz, nw;
	float tu, tv;
};

struct Meshlet
{
	uint32_t vertices[64];
	uint8_t indices[126 * 3]; // up to 126 triangles
	uint8_t triangleCount;
	uint8_t vertexCount;
};

struct Mesh {
	std::vector<Vertex> _vertices;
	std::vector<uint16_t> _indices;
#if MESHSHADER_ON
	std::vector<Vertex_MS> _verticesMS;
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
