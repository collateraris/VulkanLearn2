#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "raytracer.h"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout(set = 1, std140, binding = 1) readonly buffer ObjectBuffer{

	ObjectData objects[];
} objectBuffer;

layout (set = 0, binding = 0) readonly buffer _vertices
{
	Vertex vertices[];
} Vertices[];

layout (set = 0, binding = 1) readonly buffer _indices
{
	uint indices[];
} Indices[];

layout(set = 0, binding = 2) uniform sampler2D tex0[];

void main()
{
  ObjectData objData = objectBuffer.objects[gl_InstanceCustomIndexEXT];
  
  uint ind0 = Indices[objData.meshIndex].indices[gl_PrimitiveID * 3 + 0];
  uint ind1 = Indices[objData.meshIndex].indices[gl_PrimitiveID * 3 + 1];
  uint ind2 = Indices[objData.meshIndex].indices[gl_PrimitiveID * 3 + 2];

  Vertex v0 = Vertices[objData.meshIndex].vertices[ind0];
  Vertex v1 = Vertices[objData.meshIndex].vertices[ind1];
  Vertex v2 = Vertices[objData.meshIndex].vertices[ind2];



  prd.hitValue = vec3(0., 1., 0.0);
}
