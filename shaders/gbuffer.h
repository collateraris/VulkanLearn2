#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable

struct SGlobalCamera
{
  mat4 viewProj;     // Camera view * projection
};

struct SVertex
{
	vec4 positionXYZ_normalX;
	vec4 normalYZ_texCoordUV;
	vec4 tangentXYZ_;
};

struct SMeshlet
{
	uint dataOffset;
	uint triangleCount;
	uint vertexCount;
	uint pad;
};

struct SObjectData
{
	mat4 model;
	mat4 modelIT;//for normal
	uint meshIndex;
	uint meshletCount;
	uint diffuseTexIndex;
	int normalTexIndex;
	int metalnessTexIndex;
	int roughnessTexIndex;
	int emissionTexIndex;
	int opacityTexIndex;
	vec4 baseColorFactor;
	vec4 emissiveFactorMult_emissiveStrength;
	vec4 metallicFactor_roughnessFactor;
};