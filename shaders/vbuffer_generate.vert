#version 460
layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;

#extension GL_GOOGLE_include_directive: require

#include "vbuffer.h"


layout(set = 0, binding = 0) uniform _GlobalCamera { SGlobalCamera cameraData; };

layout(set = 0, binding = 1) readonly buffer ObjectBuffer{

	SObjectData objects[];
} objectBuffer;

layout(location = 0) flat out uint drawID;

void main()
{
	mat4 modelMatrix = objectBuffer.objects[gl_InstanceIndex].model;
	mat4 transformMatrix = (cameraData.viewProj * modelMatrix);
	gl_Position = transformMatrix * vec4(vPosition, 1.0f);
    drawID = gl_InstanceIndex;
}