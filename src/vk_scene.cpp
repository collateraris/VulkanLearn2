#include <vk_scene.h>

#include <vk_ioutils.h>

int SceneManager::addNode(Scene& scene, int parent, int level)
{
	int node = scene._hierarchy.size();

	scene._localTransforms.push_back(glm::mat4(1.0f));
	scene._globalTransforms.push_back(glm::mat4(1.0f));

	scene._hierarchy.push_back({ ._parent = parent, ._lastSibling = -1 });
	if (parent > -1)
	{
		// find first item (sibling)
		int s = scene._hierarchy[parent]._firstChild;
		if (s == -1)
		{
			scene._hierarchy[parent]._firstChild = node;
			scene._hierarchy[node]._lastSibling = node;
		}
		else
		{
			int dest = scene._hierarchy[s]._lastSibling;
			if (dest <= -1)
			{
				// no cached lastSibling, iterate nextSibling indices
				for (dest = s; scene._hierarchy[dest]._nextSibling != -1; dest = scene._hierarchy[dest]._nextSibling);
			}
			scene._hierarchy[dest]._nextSibling = node;
			scene._hierarchy[s]._lastSibling = node;
		}
	}
	scene._hierarchy[node]._level = level;
	scene._hierarchy[node]._nextSibling = -1;
	scene._hierarchy[node]._firstChild = -1;
	return node;
}

void SceneManager::loadScene(const char* fileName, Scene& scene)
{
	FILE* f = fopen(fileName, "rb");

	if (!f)
	{
		printf("Cannot open scene file '%s'. Please run SceneConverter from Chapter7 and/or MergeMeshes from Chapter 9", fileName);
		return;
	}

	uint32_t sz = 0;
	fread(&sz, sizeof(sz), 1, f);

	scene._hierarchy.resize(sz);
	scene._globalTransforms.resize(sz);
	scene._localTransforms.resize(sz);
	// TODO: check > -1
	// TODO: recalculate changedAtThisLevel() - find max depth of a node [or save scene.maxLevel]
	fread(scene._localTransforms.data(), sizeof(glm::mat4), sz, f);
	fread(scene._globalTransforms.data(), sizeof(glm::mat4), sz, f);
	fread(scene._hierarchy.data(), sizeof(Hierarchy), sz, f);

	// Mesh for node [index to some list of buffers]
	IOUtils::loadMap(f, scene._matForNode);
	IOUtils::loadMap(f, scene._meshes);

	if (!feof(f))
	{
		IOUtils::loadMap(f, scene._nameForNode);
		IOUtils::loadStringList(f, scene._names);

		IOUtils::loadStringList(f, scene._matNames);
	}

	fclose(f);
}

void SceneManager::saveScene(const char* fileName, const Scene& scene)
{
	FILE* f = fopen(fileName, "wb");

	const uint32_t sz = (uint32_t)scene._hierarchy.size();
	fwrite(&sz, sizeof(sz), 1, f);

	fwrite(scene._localTransforms.data(), sizeof(glm::mat4), sz, f);
	fwrite(scene._globalTransforms.data(), sizeof(glm::mat4), sz, f);
	fwrite(scene._hierarchy.data(), sizeof(Hierarchy), sz, f);

	// Mesh for node [index to some list of buffers]
	IOUtils::saveMap(f, scene._matForNode);
	IOUtils::saveMap(f, scene._meshes);

	if (!scene._names.empty() && !scene._nameForNode.empty())
	{
		IOUtils::saveMap(f, scene._nameForNode);
		IOUtils::saveStringList(f, scene._names);

		IOUtils::saveStringList(f, scene._matNames);
	}
	fclose(f);
}
