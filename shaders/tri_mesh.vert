#version 460
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;

layout (location = 0) out vec2 texCoord;


layout(set = 0, binding = 0) uniform  CameraBuffer{
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 frustum[6];
} cameraData;

struct ObjectData
{
	mat4 model;
	vec4 center_radius;
	uint meshletCount;
};

//all object matrices
layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{

	ObjectData objects[];
} objectBuffer;

void main()
{
	mat4 modelMatrix = objectBuffer.objects[gl_InstanceIndex].model;
	mat4 transformMatrix = (cameraData.viewproj * modelMatrix);
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
	texCoord = vTexCoord;
}