#pragma once

#include <vk_types.h>

enum MaterialFlags
{
	sMaterialFlags_CastShadow = 0x1,
	sMaterialFlags_ReceiveShadow = 0x2,
	sMaterialFlags_Transparent = 0x4,
};

class MaterialDef
{
public:
	static const uint64_t INVALID_TEXTURE = 0xFFFFFFFF;
};

struct PACKED_STRUCT MaterialDescription final
{
	gpuvec4 emissiveColor_ = { 0.0f, 0.0f, 0.0f, 0.0f };
	gpuvec4 albedoColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	// UV anisotropic roughness (isotropic lighting models use only the first value). ZW values are ignored
	gpuvec4 roughness_ = { 1.0f, 1.0f, 0.0f, 0.0f };
	float transparencyFactor_ = 1.0f;
	float alphaTest_ = 0.0f;
	float metallicFactor_ = 0.0f;
	uint32_t flags_ = sMaterialFlags_CastShadow | sMaterialFlags_ReceiveShadow;
	// maps
	uint64_t ambientOcclusionMap_ = MaterialDef::INVALID_TEXTURE;
	uint64_t emissiveMap_ = MaterialDef::INVALID_TEXTURE;
	uint64_t albedoMap_ = MaterialDef::INVALID_TEXTURE;
	/// Occlusion (R), Roughness (G), Metallic (B) https://github.com/KhronosGroup/glTF/issues/857
	uint64_t metallicRoughnessMap_ = MaterialDef::INVALID_TEXTURE;
	uint64_t normalMap_ = MaterialDef::INVALID_TEXTURE;
	uint64_t opacityMap_ = MaterialDef::INVALID_TEXTURE;
};

static_assert(sizeof(MaterialDescription) % 16 == 0, "MaterialDescription should be padded to 16 bytes");

class MaterialManager
{
public:
	static void saveMaterials(const char* fileName, const std::vector<MaterialDescription>& materials, const std::vector<std::string>& files);
	static void loadMaterials(const char* fileName, std::vector<MaterialDescription>& materials, std::vector<std::string>& files);
};