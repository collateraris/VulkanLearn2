#include <vk_mesh.h>
#include <meshoptimizer.h>
#include <iostream>

#if MESHSHADER_ON

Mesh& Mesh::remapVertexToVertexMS()
{
	_verticesMS.clear();
	for (const Vertex& vert: _vertices)
	{
		Vertex_MS vertMS;
		vertMS.vx = vert.position.x;
		vertMS.vy = vert.position.y;
		vertMS.vz = vert.position.z;

		vertMS.nx = uint8_t(vert.normal.x * 127.f + 127.f);
		vertMS.ny = uint8_t(vert.normal.y * 127.f + 127.f);
		vertMS.nz = uint8_t(vert.normal.z * 127.f + 127.f);
		vertMS.nw = 0;

		vertMS.tu = meshopt_quantizeHalf(vert.uv.x);
		vertMS.tv = meshopt_quantizeHalf(vert.uv.y);

		_verticesMS.emplace_back(vertMS);
	}

	return *this;
}

void Mesh::buildMeshlets()
{
	const size_t max_vertices = 64;
	const size_t max_triangles = 124;
	const float cone_weight = 0.0f;

	size_t max_meshlets = meshopt_buildMeshletsBound(_indices.size(), max_vertices, max_triangles);
	std::vector<meshopt_Meshlet> meshlets(max_meshlets);
	std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
	std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

	size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), _indices.data(),
		_indices.size(), &_verticesMS[0].vx, _vertices.size(), sizeof(Vertex_MS), max_vertices, max_triangles, cone_weight);

	meshlets.resize(meshlet_count);

	// TODO: we don't *really* need this but this makes sure we can assume that we need all 32 meshlets in task shader
	while (meshlets.size() % 32)
		meshlets.push_back(meshopt_Meshlet());

	meshletdata.clear();
	for (auto& meshlet : meshlets)
	{
		size_t dataOffset = meshletdata.size();

		for (unsigned int i = 0; i < meshlet.vertex_count; ++i)
			meshletdata.push_back(meshlet_vertices[meshlet.vertex_offset + i]);

		for (unsigned int i = 0; i < meshlet.triangle_count * 3; ++i)
			meshletdata.push_back(meshlet_triangles[meshlet.triangle_offset + i]);

		meshopt_Bounds bounds = meshopt_computeMeshletBounds(&meshlet_vertices[meshlet.vertex_offset], &meshlet_triangles[meshlet.triangle_offset],
			meshlet.triangle_count, &_verticesMS[0].vx, _verticesMS.size(), sizeof(Vertex_MS));

		Meshlet m = {};
		m.dataOffset = uint32_t(dataOffset);
		m.triangleCount = meshlet.triangle_count;
		m.vertexCount = meshlet.vertex_count;

		m.cone[0] = bounds.cone_axis[0];
		m.cone[1] = bounds.cone_axis[1];
		m.cone[2] = bounds.cone_axis[2];
		m.cone[3] = bounds.cone_cutoff;

		_meshlets.push_back(m);
	}
}
Mesh& Mesh::calcAddInfo()
{
	_center = glm::vec3(0);

	for (const Vertex& vert : _vertices)
	{
		_center += vert.position;
	}

	_center /= _vertices.size();

	_radius = 0;
	for (const Vertex& vert : _vertices)
	{
		_radius = std::max(_radius, glm::distance(_center, vert.position));
	}

	return *this;
}
#endif
VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	//we will have just 1 vertex buffer binding, with a per-vertex rate
	VkVertexInputBindingDescription mainBinding = {};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	//Position will be stored at Location 0
	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.offset = offsetof(Vertex, position);

	//Normal will be stored at Location 1
	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = offsetof(Vertex, normal);

	//UV will be stored at Location 2
	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 2;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset = offsetof(Vertex, uv);

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}




