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
	float vx, vy, vz;
	uint8_t nx, ny, nz, nw;
	uint16_t tu, tv;
};

struct alignas(16) Meshlet
{
	float cone[4];
	uint32_t dataOffset; // dataOffset..dataOffset+vertexCount-1 stores vertex indices, we store indices packed in 4b units after that
	uint8_t triangleCount;
	uint8_t vertexCount;
};

struct Mesh {
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	std::vector<Vertex_MS> _verticesMS;
#if MESHSHADER_ON
	std::vector<Meshlet> _meshlets;
	std::array<VkDescriptorSet, 2> meshletsSet{VK_NULL_HANDLE };
	std::vector<uint32_t> meshletdata;
#endif
	std::array<AllocatedBuffer, 2> _vertexBuffer;
#if MESHSHADER_ON
	std::array<AllocatedBuffer, 2> _meshletsBuffer;
	std::array<AllocatedBuffer, 2> _meshletdataBuffer;
#else
	std::array<AllocatedBuffer, 2> _indicesBuffer;
#endif

	bool load_from_obj(const char* filename);
#if MESHSHADER_ON
	void remapVertexToVertexMS();
	void buildMeshlets();
#endif
};
