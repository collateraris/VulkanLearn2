#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_GOOGLE_include_directive: require

#include "vbuffer.h"

layout (location = 0) out uvec2 vbuffer;

layout(early_fragment_tests) in;
void main()
{
  //gl_Layer contains objectID and meshletIndex
  uint objectID = uint(gl_Layer >> 16) + 1;
  uint meshletIndex = uint(gl_Layer & 0xFFFF);
	vbuffer.xy = packVBuffer(objectID, meshletIndex, uint(gl_PrimitiveID));
}