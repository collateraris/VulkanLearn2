#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "raytracer.h"

layout(location = 0) rayPayloadInEXT hitPayload prd;

hitAttributeEXT vec2 baryCoord;

layout(set = 1, binding = 1) readonly buffer ObjectBuffer{

	ObjectData objects[];
} objectBuffer;

layout (set = 0, std140, binding = 0) readonly buffer _vertices
{
	Vertex vertices[];
} Vertices[];

layout(set = 0, binding = 1) uniform sampler2D tex0[];

void main()
{
  ObjectData objData = objectBuffer.objects[gl_InstanceID];
  

  Vertex v0 = Vertices[objData.meshIndex].vertices[gl_PrimitiveID * 3 + 0];
  Vertex v1 = Vertices[objData.meshIndex].vertices[gl_PrimitiveID * 3 + 1];
  Vertex v2 = Vertices[objData.meshIndex].vertices[gl_PrimitiveID * 3 + 2];

  const vec3 barycentrics = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);

   vec2 texCoord =
        v0.normalYZ_texCoordUV.zw * barycentrics.x + v1.normalYZ_texCoordUV.zw * barycentrics.y + v2.normalYZ_texCoordUV.zw * barycentrics.z;

  vec3 diffuse = texture(tex0[objData.diffuseTexIndex], texCoord).xyz;

  prd.hitValue = diffuse;
}
