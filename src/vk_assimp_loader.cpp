#include <vk_assimp_loader.h>

#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

glm::mat4 toMat4(const aiMatrix4x4& from);

void traverse(const aiScene* sourceScene, Scene& scene, aiNode* anode, int parent, int level);

void AsimpLoader::processScene(const SceneConfig& config, Scene& newScene) 
{
	const std::size_t pathSeparator = config.fileName.find_last_of("/\\");
	const std::string basePath = (pathSeparator != std::string::npos) ? config.fileName.substr(0, pathSeparator + 1) : std::string();

	const unsigned int flags = 0 |
		aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_LimitBoneWeights |
		aiProcess_SplitLargeMeshes |
		aiProcess_ImproveCacheLocality |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_FindDegenerates |
		aiProcess_FindInvalidData |
		aiProcess_GenUVCoords;

	printf("Loading scene from '%s'...\n", config.fileName.c_str());

	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(basePath, flags);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode || !scene->HasMeshes())
	{
		printf("Unable to load '%s'\n", config.fileName.c_str());
		exit(EXIT_FAILURE);
	}

	traverse(scene, newScene, scene->mRootNode, -1, 0);
}

glm::mat4 toMat4(const aiMatrix4x4& from)
{
	glm::mat4 to;
	to[0][0] = (float)from.a1; to[0][1] = (float)from.b1;  to[0][2] = (float)from.c1; to[0][3] = (float)from.d1;
	to[1][0] = (float)from.a2; to[1][1] = (float)from.b2;  to[1][2] = (float)from.c2; to[1][3] = (float)from.d2;
	to[2][0] = (float)from.a3; to[2][1] = (float)from.b3;  to[2][2] = (float)from.c3; to[2][3] = (float)from.d3;
	to[3][0] = (float)from.a4; to[3][1] = (float)from.b4;  to[3][2] = (float)from.c4; to[3][3] = (float)from.d4;
	return to;
}

void traverse(const aiScene* sourceScene, Scene& scene, aiNode* anode, int parent, int level)
{
	int newNode = SceneManager::addNode(scene, parent, level);

	if (anode->mName.C_Str())
	{
		uint32_t stringID = (uint32_t)scene._names.size();
		scene._names.push_back(std::string(anode->mName.C_Str()));
		scene._nameForNode[newNode] = stringID;
	}

	for (size_t i = 0; i < anode->mNumMeshes; i++)
	{
		int newSubNode = SceneManager::addNode(scene, newNode, level + 1);

		uint32_t stringID = (uint32_t)scene._names.size();
		scene._names.push_back(std::string(anode->mName.C_Str()) + "_Mesh_" + std::to_string(i));
		scene._nameForNode[newSubNode] = stringID;

		int mesh = (int)anode->mMeshes[i];
		scene._meshes[newSubNode] = mesh;
		scene._matForNode[newSubNode] = sourceScene->mMeshes[mesh]->mMaterialIndex;

		scene._globalTransforms[newSubNode] = glm::mat4(1.0f);
		scene._localTransforms[newSubNode] = glm::mat4(1.0f);
	}

	scene._globalTransforms[newNode] = glm::mat4(1.0f);
	scene._localTransforms[newNode] = toMat4(anode->mTransformation);

	for (unsigned int n = 0; n < anode->mNumChildren; n++)
		traverse(sourceScene, scene, anode->mChildren[n], newNode, level + 1);
}
