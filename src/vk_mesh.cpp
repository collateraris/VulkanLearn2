#include <vk_mesh.h>

#include <tiny_obj_loader.h>
#include <iostream>
bool Mesh::load_from_obj(const char* filename)
{
	//attrib will contain the vertex arrays of the file
	tinyobj::attrib_t attrib;
	//shapes contains the info for each separate object in the file
	std::vector<tinyobj::shape_t> shapes;
	//materials contains the information about the material of each shape, but we won't use it.
	std::vector<tinyobj::material_t> materials;

	//error and warning output from the load function
	std::string warn;
	std::string err;

	//load the OBJ file
	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);
	//make sure to output the warnings to the console, in case there are issues with the file
	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}
	//if we have any error, print it to the console, and break the mesh loading.
	//This happens if the file can't be found or is malformed
	if (!err.empty()) {
		std::cerr << err << std::endl;
		return false;
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {

			//hardcode loading to triangles
			int fv = 3;

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				//vertex normal
				tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
				tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
				tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
				//vertex uv
				tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
				tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

				//copy it into our vertex
				Vertex new_vert;
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;

				new_vert.uv.x = ux;
				new_vert.uv.y = 1 - uy;

				_vertices.push_back(new_vert);
			}
			index_offset += fv;
		}
	}

	return true;
}

void Mesh::buildMeshlets()
{
	Meshlet meshlet = {};
	_meshlets.push_back(meshlet);


	const glm::vec3 vertices[3] = { glm::vec3(-1,-1,0), glm::vec3(0,1,0), glm::vec3(1,-1,0) };
	const glm::vec3 colors[3] = { glm::vec3(1.0,0.0,0.0), glm::vec3(0.0,1.0,0.0), glm::vec3(0.0,0.0,1.0) };

	_vertices.resize(3);

	_vertices[0].position = vertices[0];
	_vertices[1].position = vertices[1];
	_vertices[1].position = vertices[2];

	//std::vector<uint32_t> meshletVertices(_vertices.size(), 0xff);

	//for (size_t i = 0; i < _indices.size(); i += 3)
	//{
	//	uint32_t a = _indices[i + 0];
	//	uint32_t b = _indices[i + 1];
	//	uint32_t c = _indices[i + 2];

	//	uint32_t& av = meshletVertices[a];
	//	uint32_t& bv = meshletVertices[b];
	//	uint32_t& cv = meshletVertices[c];

	//	if (meshlet.vertexCount + (av == 0xff) + (bv == 0xff) + (cv == 0xff) > 64 || meshlet.indexCount + 3 > 126)
	//	{
	//		_meshlets.push_back(meshlet);

	//		for (size_t j = 0; j < meshlet.vertexCount; ++j)
	//			meshletVertices[meshlet.vertices[j]] = 0xff;

	//		meshlet = {};
	//	}

	//	if (av == 0xff)
	//	{
	//		av = meshlet.vertexCount;
	//		meshlet.vertices[meshlet.vertexCount++] = a;
	//	}

	//	if (bv == 0xff)
	//	{
	//		bv = meshlet.vertexCount;
	//		meshlet.vertices[meshlet.vertexCount++] = b;
	//	}

	//	if (cv == 0xff)
	//	{
	//		cv = meshlet.vertexCount;
	//		meshlet.vertices[meshlet.vertexCount++] = c;
	//	}

	//	meshlet.indices[meshlet.indexCount++] = av;
	//	meshlet.indices[meshlet.indexCount++] = bv;
	//	meshlet.indices[meshlet.indexCount++] = cv;
	//}

	//if (meshlet.indexCount)
	//	_meshlets.push_back(meshlet);
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




