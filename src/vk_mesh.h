#pragma once

#include <vk_types.h>

struct VertexInputDescription {

	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;

	VkPipelineVertexInputStateCreateFlags flags = 0;
};


struct Vertex {

	glm::vec4 positionXYZ_normalX;
	glm::vec4 normalYZ_texCoordUV;

	static VertexInputDescription get_vertex_description();
};

struct Vertex_MS
{
	float vx, vy, vz;
	uint8_t nx, ny, nz, nw;
	uint16_t tu, tv;
};

struct Vertex_VisB
{
	glm::vec4 position;
};

struct Meshlet
{
	uint32_t dataOffset; // dataOffset..dataOffset+vertexCount-1 stores vertex indices, we store indices packed in 4b units after that
	uint8_t triangleCount;
	uint8_t vertexCount;
};

struct alignas(16) GPUObjectData {
	glm::mat4 modelMatrix;
	glm::vec4 center_radius;
	uint32_t meshletCount;
	uint32_t meshIndex;
	uint32_t diffuseTexIndex;
	uint32_t pad;
};

struct Mesh {
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	std::vector<Vertex_MS> _verticesMS;
#if VBUFFER_ON
	std::vector<Vertex_VisB> _verticesVisB;
#endif
#if MESHSHADER_ON || GBUFFER_ON
	std::vector<Meshlet> _meshlets;
	VkDescriptorSet meshletsSet{VK_NULL_HANDLE };
	std::vector<uint32_t> meshletdata;
#endif
	AllocatedBuffer _vertexBuffer;
#if MESHSHADER_ON || GBUFFER_ON
	AllocatedBuffer _meshletsBuffer;
	AllocatedBuffer _meshletdataBuffer;
#endif
#if INDIRECT_DRAW_ON || VBUFFER_ON
	AllocatedBuffer _indicesBuffer;
#endif
#if RAYTRACER_ON
	AllocatedBuffer _vertexBufferRT;
	AllocatedBuffer _indicesBufferRT;
#endif
#if VBUFFER_ON
	AllocatedBuffer _vertexBufferVisB;
#endif
	glm::vec3 _center = glm::vec3(0);
	float _radius = 0;

#if MESHSHADER_ON || GBUFFER_ON
	Mesh& remapVertexToVertexMS();
	void buildMeshlets();
#endif
#if VBUFFER_ON
	void remapVertexToVertexVisB();
#endif
	Mesh& calcAddInfo();
};
