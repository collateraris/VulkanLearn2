#version 460

#extension GL_EXT_nonuniform_qualifier:enable

layout (location = 0) in PerVertexData
{
  flat uint drawID;
} fragIn;

layout(set = 2, binding = 0, rg32ui) uniform uimage2D vbuffer;

layout(early_fragment_tests) in;
void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    imageStore(vbuffer, coord, uvec4(fragIn.drawID, gl_PrimitiveID, fragIn.drawID, fragIn.drawID));
}