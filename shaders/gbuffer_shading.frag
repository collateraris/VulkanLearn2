#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable

#extension GL_GOOGLE_include_directive: require

#include "gbuffer_shading.h"

//shader input
layout (location = 0) in vec2 texCoord;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D texSet[];

layout(set = 1, binding = 0) uniform _GlobalCamera { SGlobalCamera cameraData; };

layout(set = 1, binding = 1) readonly buffer ObjectBuffer{

	SObjectData objects[];
} objectBuffer;

layout(set = 2, binding = 0) uniform sampler2D wposTex;
layout(set = 2, binding = 1) uniform sampler2D normalTex;
layout(set = 2, binding = 2) uniform sampler2D uvTex;
layout(set = 2, binding = 3) uniform usampler2D objIDTex;

void main()
{
	uint objID = texture(objIDTex, texCoord).r;
    uint diffuseID = objectBuffer.objects[objID].diffuseTexIndex;
    vec2 gbufferTexCoord = texture(uvTex, texCoord).rg;
    vec3 diffuse = texture(texSet[diffuseID], gbufferTexCoord).rgb;
	outFragColor = vec4(diffuse,1.0f);
}