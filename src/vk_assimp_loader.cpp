#include <vk_assimp_loader.h>

#include <assimp/cimport.h>
#include <assimp/material.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

glm::mat4 toMat4(const aiMatrix4x4& from);

void traverse(const aiScene* sourceScene, Scene& scene, aiNode* anode, int parent, int level, glm::mat4 model);

void collectAIMesh(const aiMesh* amesh, const SceneConfig& config, ResourceManager& resManager);

void collectAIMaterialDescAndTexture(const aiMaterial* amat, ResourceManager& resManager, std::string lastDirectory);

void AsimpLoader::processScene(const SceneConfig& config, Scene& newScene, ResourceManager& resManager, glm::mat4 model/* = glm::mat4(1.0)*/)
{
	std::filesystem::path filePath(config.fileName);
	if (!std::filesystem::exists(filePath))
	{
		printf("file path does not exist '%s'\n", config.fileName.c_str());
		exit(EXIT_FAILURE);
	}

	std::string path(config.fileName.cbegin(), config.fileName.cend());

	const unsigned int flags = 0 |
		aiProcess_Triangulate |
		aiProcess_MakeLeftHanded |
		aiProcess_GenSmoothNormals |
		aiProcess_LimitBoneWeights |
		aiProcess_SplitLargeMeshes |
		aiProcess_ImproveCacheLocality |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_FindDegenerates |
		aiProcess_FindInvalidData |
		aiProcess_GenUVCoords |
		aiProcess_Triangulate |
		aiProcess_CalcTangentSpace;

	printf("Loading scene from '%s'...\n", config.fileName.c_str());

	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, flags);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode || !scene->HasMeshes())
	{
		printf("Unable to load '%s'\n", config.fileName.c_str());
		exit(EXIT_FAILURE);
	}

	for (unsigned int i = 0; i != scene->mNumMeshes; i++)
	{
		collectAIMesh(scene->mMeshes[i], config, resManager);
	}

	std::string lastDirectory = path.substr(0, path.find_last_of('/'));

	for (unsigned int m = 0; m < scene->mNumMaterials; m++)
	{
		collectAIMaterialDescAndTexture(scene->mMaterials[m], resManager, lastDirectory);
	}

	traverse(scene, newScene, scene->mRootNode, -1, 0, model);
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

void traverse(const aiScene* sourceScene, Scene& scene, aiNode* anode, int parent, int level, glm::mat4 accTransform)
{
	int newNode = SceneManager::addNode(scene, parent, level);

	glm::mat4 transform = glm::mat4(1.0f);

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
		scene._localTransforms[newSubNode] = accTransform;
	}

	transform = accTransform * toMat4(anode->mTransformation);

	for (unsigned int n = 0; n < anode->mNumChildren; n++)
		traverse(sourceScene, scene, anode->mChildren[n], newNode, level + 1, transform);
}

void collectAIMesh(const aiMesh* amesh, const SceneConfig& config, ResourceManager& resManager)
{
	resManager.meshList.push_back(std::make_unique<Mesh>());
	Mesh* newMesh = resManager.meshList.back().get();

	const bool hasTexCoords = amesh->HasTextureCoords(0);

	for (size_t i = 0; i != amesh->mNumVertices; i++)
	{
		const aiVector3D v = amesh->mVertices[i];
		const aiVector3D n = amesh->mNormals[i];
		const aiVector3D t = hasTexCoords ? amesh->mTextureCoords[0][i] : aiVector3D();
		const aiVector3D tan = amesh->mTangents[i];

		Vertex meshData;
		meshData.positionXYZ_normalX = glm::vec4(v.x * config.scaleFactor, v.y * config.scaleFactor, v.z * config.scaleFactor, n.x);
		meshData.normalYZ_texCoordUV = glm::vec4(n.y, n.z, t.x, 1. - t.y);
		meshData.tangentXYZ_ = glm::vec4(tan.x, tan.y, tan.z, 1.);

		resManager.maxCube.x = std::max(resManager.maxCube.x, meshData.positionXYZ_normalX.x);
		resManager.maxCube.y = std::max(resManager.maxCube.y, meshData.positionXYZ_normalX.y);
		resManager.maxCube.z = std::max(resManager.maxCube.z, meshData.positionXYZ_normalX.z);

		resManager.minCube.x = std::min(resManager.minCube.x, meshData.positionXYZ_normalX.x);
		resManager.minCube.y = std::min(resManager.minCube.y, meshData.positionXYZ_normalX.y);
		resManager.minCube.z = std::min(resManager.minCube.z, meshData.positionXYZ_normalX.z);

		newMesh->_vertices.push_back(meshData);
	}

	for (unsigned int i = 0; i < amesh->mNumFaces; i++)
	{
		if (amesh->mFaces[i].mNumIndices != 3)
			continue;
		for (unsigned j = 0; j != amesh->mFaces[i].mNumIndices; j++)
		{
			newMesh->_indices.push_back(amesh->mFaces[i].mIndices[j]);
		}
	}
}

void ProcessMeshLoadMaterialTextures(const aiMaterial* mat, aiTextureType type, std::string lastDirectory, MaterialDesc* newMatDesc, ResourceManager& resManager)
{
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		std::string texPath = lastDirectory + "/" + std::string(str.C_Str());

		switch (type)
		{
		case aiTextureType_BASE_COLOR:
		case aiTextureType_DIFFUSE:
			if (newMatDesc->diffuseTexture.empty())
			{
				newMatDesc->diffuseTexture = texPath;
				newMatDesc->diffuseTextureIndex = resManager.store_texture(newMatDesc->diffuseTexture);
			}
			break;
		case aiTextureType_SPECULAR:
			break;
		case aiTextureType_HEIGHT:
		case aiTextureType_NORMALS:
			if (newMatDesc->normalTexture.empty())
			{
				newMatDesc->normalTexture = texPath;
				newMatDesc->normalTextureIndex = resManager.store_texture(newMatDesc->normalTexture);
			}
			break;
		case aiTextureType_EMISSION_COLOR:
		case aiTextureType_EMISSIVE:
			if (newMatDesc->emissionTexture.empty())
			{
				newMatDesc->emissionTexture = texPath;
				newMatDesc->emissionTextureIndex = resManager.store_texture(newMatDesc->emissionTexture);
			}
			break;
		case aiTextureType_METALNESS:
			if (newMatDesc->metalnessTexture.empty())
			{
				newMatDesc->metalnessTexture = texPath;
				newMatDesc->metalnessTextureIndex = resManager.store_texture(newMatDesc->metalnessTexture);
			}
			break;
		case aiTextureType_DIFFUSE_ROUGHNESS:
			if (newMatDesc->roughnessTexture.empty())
			{
				newMatDesc->roughnessTexture = texPath;
				newMatDesc->roughnessTextureIndex = resManager.store_texture(newMatDesc->roughnessTexture);
			}
			break;
		case aiTextureType_OPACITY:
			if (newMatDesc->opacityTexture.empty())
			{
				newMatDesc->opacityTexture = texPath;
				newMatDesc->opacityTextureIndex = resManager.store_texture(newMatDesc->opacityTexture);
			}
			break;
		case aiTextureType_AMBIENT:
			break;
		default:
			continue;
		}
	}
}

void collectAIMaterialDescAndTexture(const aiMaterial* amat, ResourceManager& resManager, std::string lastDirectory)
{
	resManager.matDescList.push_back(std::make_unique<MaterialDesc>());
	MaterialDesc* newMatDesc = resManager.matDescList.back().get();

	newMatDesc->matName = amat->GetName().C_Str();

	ProcessMeshLoadMaterialTextures(amat, aiTextureType_DIFFUSE, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_HEIGHT, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_OPACITY, lastDirectory, newMatDesc, resManager);
	// or PBR

	ProcessMeshLoadMaterialTextures(amat, aiTextureType_BASE_COLOR, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_NORMALS, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_EMISSIVE, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_EMISSION_COLOR, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_METALNESS, lastDirectory, newMatDesc, resManager);
	ProcessMeshLoadMaterialTextures(amat, aiTextureType_DIFFUSE_ROUGHNESS, lastDirectory, newMatDesc, resManager);

	aiColor4D color(0.f, 0.f, 0.f, 0.f);
	if (amat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, color) == aiReturn_SUCCESS)
	{
		newMatDesc->baseColorFactor = glm::vec4(color.r, color.g, color.b, color.a);
	}

	float opacity = 1;
	if (amat->Get(AI_MATKEY_OPACITY, opacity) == aiReturn_SUCCESS)
	{
		newMatDesc->baseColorFactor.a = opacity;
	}

	aiColor3D emissiveFactor(0., 0., 0.);  
	if (amat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveFactor) == aiReturn_SUCCESS)
	{
		newMatDesc->emissiveFactorMult_emissiveStrength = glm::vec4(emissiveFactor.r, emissiveFactor.g, emissiveFactor.b, 1);
	}

	float emissiveStrenght(0.f);
	if (amat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrenght) == aiReturn_SUCCESS)
	{
		newMatDesc->emissiveFactorMult_emissiveStrength *= emissiveStrenght;
	}

	float metallicFactor(0.f);
	if (amat->Get(AI_MATKEY_METALLIC_FACTOR, metallicFactor) == aiReturn_SUCCESS)
	{
		newMatDesc->metallicFactor_roughnessFactor.x *= metallicFactor;
	}

	float roughnessFactor(0.);
	if (amat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessFactor) == aiReturn_SUCCESS)
	{
		newMatDesc->metallicFactor_roughnessFactor.y *= roughnessFactor;
	}

}
