#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in PerVertexData
{
  vec3 wpos;  
  flat uint drawID;
} fragIn;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out uint outObjID;

layout(early_fragment_tests) in;
void main()
{
	outPos = fragIn.wpos;
    outNormal = vec3(1., 0., 0.);
    outUV = vec2(0.5, 0.5);
    outObjID = fragIn.drawID;
}