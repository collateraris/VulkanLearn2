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

layout(set = 1, binding = 0) uniform sampler2D giTex;

void main()
{
    vec3 gi = texture(giTex, texCoord).rgb;

    const float gamma = 2.2;
    vec3 mapped = gi / (gi + vec3(1.0));
    mapped = pow(mapped, vec3(1.0 / gamma));
	outFragColor = vec4(mapped,1.0f);
}