struct SGlobalCamera
{
  mat4 viewProj;     // Camera view * projection
};

struct SVertex {

	vec4 positionXYZ_normalX;
	vec4 normalYZ_texCoordUV;
};

struct SObjectData
{
    mat4 model;
    uint meshIndex;
    uint diffuseTexIndex;
    uint pad1;
    uint pad2;
};

struct SDerivativesOutput
{
	vec3 dbDx;
	vec3 dbDy;
};