#include <vk_mesh.h>
#include <meshoptimizer.h>
#include <objparser.h>
#include <iostream>
bool Mesh::load_from_obj(const char* filename)
{
#if MESHSHADER_ON
	ObjFile file;
	if (!objParseFile(file, filename))
		return false;

	size_t index_count = file.f_size / 3;

	std::vector<Vertex_MS> vertices(index_count);

	for (size_t i = 0; i < index_count; ++i)
	{
		Vertex_MS& v = vertices[i];

		int vi = file.f[i * 3 + 0];
		int vti = file.f[i * 3 + 1];
		int vni = file.f[i * 3 + 2];

		float nx = vni < 0 ? 0.f : file.vn[vni * 3 + 0];
		float ny = vni < 0 ? 0.f : file.vn[vni * 3 + 1];
		float nz = vni < 0 ? 1.f : file.vn[vni * 3 + 2];

		v.vx = meshopt_quantizeHalf(file.v[vi * 3 + 0]);
		v.vy = meshopt_quantizeHalf(file.v[vi * 3 + 1]);
		v.vz = meshopt_quantizeHalf(file.v[vi * 3 + 2]);
		v.vw = 0;
		v.nx = uint8_t(nx * 127.f + 127.f); // TODO: fix rounding
		v.ny = uint8_t(ny * 127.f + 127.f); // TODO: fix rounding
		v.nz = uint8_t(nz * 127.f + 127.f); // TODO: fix rounding
		v.tu = meshopt_quantizeHalf(vti < 0 ? 0.f : file.vt[vti * 3 + 0]);
		v.tv = meshopt_quantizeHalf(vti < 0 ? 0.f : file.vt[vti * 3 + 1]);
	}

	std::vector<uint32_t> remap(index_count);
	size_t vertex_count = meshopt_generateVertexRemap(remap.data(), 0, index_count, vertices.data(), index_count, sizeof(Vertex_MS));

	_verticesMS.resize(vertex_count);
	_indices.resize(index_count);

	meshopt_remapVertexBuffer(_verticesMS.data(), vertices.data(), index_count, sizeof(Vertex_MS), remap.data());
	meshopt_remapIndexBuffer(_indices.data(), 0, index_count, remap.data());

	meshopt_optimizeVertexCache(_indices.data(), _indices.data(), index_count, vertex_count);
	meshopt_optimizeVertexFetch(_verticesMS.data(), _indices.data(), index_count, _verticesMS.data(), vertex_count, sizeof(Vertex_MS));
#endif
	// TODO: optimize the mesh for more efficient GPU rendering
	return true;
}
#if MESHSHADER_ON

void Mesh::remapVertexToVertexMS()
{
	_verticesMS.clear();
	_indices.clear();
	size_t index_count = _vertices.size();
	index_count -= index_count % 3;
	std::vector<Vertex_MS> vertices{};
	for (int i = 0; i < index_count; i++)
	{
		Vertex& vert = _vertices[i];
		Vertex_MS vertMS;
		vertMS.vx = meshopt_quantizeHalf(vert.position.x);
		vertMS.vy = meshopt_quantizeHalf(vert.position.y);
		vertMS.vz = meshopt_quantizeHalf(vert.position.z);
		vertMS.vw = 0;

		vertMS.nx = uint8_t(vert.normal.x * 127.f + 127.f);
		vertMS.ny = uint8_t(vert.normal.y * 127.f + 127.f);
		vertMS.nz = uint8_t(vert.normal.z * 127.f + 127.f);
		vertMS.nw = 0;

		vertMS.tu = meshopt_quantizeHalf(vert.uv.x);
		vertMS.tv = meshopt_quantizeHalf(vert.uv.y);

		vertices.emplace_back(vertMS);
	}


	std::vector<uint32_t> remap(index_count);
	size_t vertex_count = meshopt_generateVertexRemap(remap.data(), 0, index_count, vertices.data(), index_count, sizeof(Vertex_MS));

	_verticesMS.resize(vertex_count);
	_indices.resize(index_count);

	meshopt_remapVertexBuffer(_verticesMS.data(), vertices.data(), index_count, sizeof(Vertex_MS), remap.data());
	meshopt_remapIndexBuffer(_indices.data(), 0, index_count, remap.data());

	meshopt_optimizeVertexCache(_indices.data(), _indices.data(), index_count, vertex_count);
	meshopt_optimizeVertexFetch(_verticesMS.data(), _indices.data(), index_count, _verticesMS.data(), vertex_count, sizeof(Vertex_MS));
}

void Mesh::buildMeshlets()
{
	Meshlet meshlet = {};

	std::vector<uint8_t> meshletVertices(_verticesMS.size(), 0xff);

	for (size_t i = 0; i < _indices.size(); i += 3)
	{
		auto a = _indices[i + 0];
		auto b = _indices[i + 1];
		auto c = _indices[i + 2];

		auto& av = meshletVertices[a];
		auto& bv = meshletVertices[b];
		auto& cv = meshletVertices[c];

		if (meshlet.vertexCount + (av == 0xff) + (bv == 0xff) + (cv == 0xff) > 64 || meshlet.triangleCount >= 126)
		{
			_meshlets.push_back(meshlet);

			for (size_t j = 0; j < meshlet.vertexCount; ++j)
				meshletVertices[meshlet.vertices[j]] = 0xff;

			meshlet = {};
		}

		if (av == 0xff)
		{
			av = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = a;
		}

		if (bv == 0xff)
		{
			bv = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = b;
		}

		if (cv == 0xff)
		{
			cv = meshlet.vertexCount;
			meshlet.vertices[meshlet.vertexCount++] = c;
		}

		meshlet.indices[meshlet.triangleCount * 3 + 0] = av;
		meshlet.indices[meshlet.triangleCount * 3 + 1] = bv;
		meshlet.indices[meshlet.triangleCount * 3 + 2] = cv;
		meshlet.triangleCount++;
	}

	if (meshlet.triangleCount)
		_meshlets.push_back(meshlet);
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




