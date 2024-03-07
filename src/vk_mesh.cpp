#include <vk_mesh.h>
#include <meshoptimizer.h>
#include <iostream>

#if MESHSHADER_ON || GBUFFER_ON || VBUFFER_ON

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
		_indices.size(), &_vertices[0].positionXYZ_normalX.x, _vertices.size(), sizeof(Vertex), max_vertices, max_triangles, cone_weight);

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
#if !VBUFFER_ON && !GBUFFER_ON
		glm::vec3 center = glm::vec3(0);
		float radius = 0;

		float aabb_min_x, aabb_min_y, aabb_min_z;
		float aabb_max_x, aabb_max_y, aabb_max_z;
		size_t vertex_index = meshlet_vertices[meshlet.vertex_offset];
		aabb_min_x = aabb_max_x = _verticesMS[vertex_index].vx;
		aabb_min_y = aabb_max_y = _verticesMS[vertex_index].vy;
		aabb_min_z = aabb_max_z = _verticesMS[vertex_index].vz;

		for (unsigned int i = 0; i < meshlet.vertex_count; ++i)
		{
			const Vertex_MS& vert = _verticesMS[meshlet_vertices[meshlet.vertex_offset]];

			center += glm::vec3(vert.vx, vert.vy, vert.vz);

			aabb_min_x = std::min(aabb_min_x, vert.vx);
			aabb_min_y = std::min(aabb_min_y, vert.vy);
			aabb_min_z = std::min(aabb_min_z, vert.vz);

			aabb_max_x = std::max(aabb_max_x, vert.vx);
			aabb_max_y = std::max(aabb_max_y, vert.vy);
			aabb_max_z = std::max(aabb_max_z, vert.vz);
		}

		for (unsigned int i = 0; i < meshlet.vertex_count; ++i)
		{
			const Vertex_MS& vert = _verticesMS[meshlet_vertices[meshlet.vertex_offset]];

			glm::vec3 position = glm::vec3(vert.vx, vert.vy, vert.vz);
			radius = std::max(radius, glm::distance(center, position));
		}
#endif

		Meshlet m = {};
		m.dataOffset = uint32_t(dataOffset);
		m.triangleCount = meshlet.triangle_count;
		m.vertexCount = meshlet.vertex_count;
#if !VBUFFER_ON && !GBUFFER_ON
		m.center_radius[0] = center.x;
		m.center_radius[1] = center.y;
		m.center_radius[2] = center.z;
		m.center_radius[3] = radius;

		m.aabb_min[0] = aabb_min_x;
		m.aabb_min[1] = aabb_min_y;
		m.aabb_min[2] = aabb_min_z;

		m.aabb_max[0] = aabb_max_x;
		m.aabb_max[1] = aabb_max_y;
		m.aabb_max[2] = aabb_max_z;
#endif
		_meshlets.push_back(m);
	}
}
#endif
Mesh& Mesh::calcAddInfo()
{
	_center = glm::vec3(0);

	for (const Vertex& vert : _vertices)
	{
		_center += glm::vec3(vert.positionXYZ_normalX);
	}

	_center /= _vertices.size();

	_radius = 0;
	for (const Vertex& vert : _vertices)
	{
		_radius = std::max(_radius, glm::distance(_center, glm::vec3(vert.positionXYZ_normalX)));
	}

	return *this;
}
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
	positionAttribute.offset = 0;

	//Normal will be stored at Location 1
	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = 4 * 3;

	//UV will be stored at Location 2
	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 2;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset = 4 * 3;

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}




