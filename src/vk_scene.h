#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

class SceneDef
{
public:
	static const int SCENE_MAX_NODE_LEVEL = 16;
};

struct Hierarchy
{
	int _parent;
	int _firstChild;
	int _nextSibling;
	int _lastSibling;
	int _level;
};

struct Scene
{
	std::vector<glm::mat4> _localTransforms;
	std::vector<glm::mat4> _globalTransforms;
	std::vector<Hierarchy> _hierarchy;

	// list of nodes whose global transform must be recalculated
	std::vector<int> _changedAtThisFrame[SceneDef::SCENE_MAX_NODE_LEVEL];

	// Node -> Mesh
	std::unordered_map<uint32_t, uint32_t> _meshes;
	
	// Node -> Material
	std::unordered_map<uint32_t, uint32_t> _matForNode;

	//debug
	std::unordered_map<uint32_t, uint32_t> _nameForNode;

	std::vector<std::string> _names;

	std::vector<std::string> _matNames;
};

class SceneManager
{
public:

	static int addNode(Scene& scene, int parent, int level);

	static void markAsChanged(Scene& scene, int node);

	static void recalculateGlobalTransforms(Scene& scene);

	static void loadScene(const char* fileName, Scene& scene);
	static void saveScene(const char* fileName, const Scene& scene);
};