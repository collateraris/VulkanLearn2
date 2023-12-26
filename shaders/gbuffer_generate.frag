#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in PerVertexData
{
  vec3 wpos;
  vec3 normal;
  vec2 uv;  
  flat uint drawID;
} fragIn;

layout (location = 0) out vec4 outPos;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) out uint outObjID;

layout(early_fragment_tests) in;
void main()
{
	  outPos = vec4(fragIn.wpos, 1.);
    outNormal = vec4(fragIn.normal, 1.);
    outUV = fragIn.uv;
    outObjID = fragIn.drawID;
}