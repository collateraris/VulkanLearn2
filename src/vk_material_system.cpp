#include <vk_material_system.h>

#include <vk_ioutils.h>

void MaterialManager::saveMaterials(const char* fileName, const std::vector<MaterialDescription>& materials, const std::vector<std::string>& files)
{
	FILE* f = fopen(fileName, "wb");
	if (!f)
		return;

	uint32_t sz = (uint32_t)materials.size();
	fwrite(&sz, 1, sizeof(uint32_t), f);
	fwrite(materials.data(), sizeof(MaterialDescription), sz, f);
	IOUtils::saveStringList(f, files);
	fclose(f);
}

void MaterialManager::loadMaterials(const char* fileName, std::vector<MaterialDescription>& materials, std::vector<std::string>& files)
{
	FILE* f = fopen(fileName, "rb");
	if (!f) {
		printf("Cannot load file %s\nPlease run SceneConverter tool from Chapter7\n", fileName);
		exit(255);
	}

	uint32_t sz;
	fread(&sz, 1, sizeof(uint32_t), f);
	materials.resize(sz);
	fread(materials.data(), sizeof(MaterialDescription), materials.size(), f);
	IOUtils::loadStringList(f, files);
	fclose(f);
}
