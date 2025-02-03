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

struct Vertex_VisB
{
	glm::vec4 position;
};

struct alignas(16) Meshlet
{
	uint32_t dataOffset; // dataOffset..dataOffset+vertexCount-1 stores vertex indices, we store indices packed in 4b units after that
	uint32_t triangleCount;
	uint32_t vertexCount;
	uint32_t pad;
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
#if MESHSHADER_ON || GBUFFER_ON || VBUFFER_ON
	std::vector<Meshlet> _meshlets;
	VkDescriptorSet meshletsSet{VK_NULL_HANDLE };
	std::vector<uint32_t> meshletdata;
#endif
	AllocatedBuffer _vertexBuffer;
#if MESHSHADER_ON || GBUFFER_ON || VBUFFER_ON
	AllocatedBuffer _meshletsBuffer;
	AllocatedBuffer _meshletdataBuffer;
#endif
#if RAYTRACER_ON
	AllocatedBuffer _indicesBuffer;
#endif
	AllocatedBuffer _vertexBufferRT;
#if RAYTRACER_ON
	AllocatedBuffer _indicesBufferRT;
#endif
	glm::vec3 _center = glm::vec3(0);
	float _radius = 0;

#if MESHSHADER_ON || GBUFFER_ON || VBUFFER_ON
	void buildMeshlets();
#endif
	Mesh& calcAddInfo();
};
