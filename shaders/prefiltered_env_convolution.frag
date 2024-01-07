#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_GOOGLE_include_directive : enable

#include "ibl.h"

layout (location = 0) in vec3 localPos;

layout (location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform PushConsts {
	layout (offset = 64) float roughness;
} consts;

void main()
{		
    vec3 N = normalize(localPos);    
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float totalWeight = 0.0;   
    vec3 prefilteredColor = vec3(0.0);     
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, consts.roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            prefilteredColor += texture(environmentMap, L).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }
    FragColor = vec4(prefilteredColor / totalWeight, 1.f);
};