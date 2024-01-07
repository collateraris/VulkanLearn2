#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_GOOGLE_include_directive : enable

#include "ibl.h"

layout (location = 0) in vec2 TexCoords;

layout (location = 0) out vec2 FragColor;

void main()
{		
    vec2 integratedBRDF = IntegrateBRDF(TexCoords.x, TexCoords.y);
    FragColor = integratedBRDF;
};

