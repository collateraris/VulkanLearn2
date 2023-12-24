#version 460

#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_GOOGLE_include_directive: require

#include "vbuffer_shading.h"

//shader input
layout (location = 0) in vec2 texCoord;
//output write
layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) readonly buffer _vertices
{
	SVertex vertices[];
} Vertices[];

layout(set = 0, binding = 1) uniform sampler2D texSet[];

layout(set = 1, binding = 0) uniform _GlobalCamera { SGlobalCamera cameraData; };

layout(set = 1, binding = 1) readonly buffer ObjectBuffer{

	SObjectData objects[];
} objectBuffer;

layout(set = 2, binding = 0) uniform usampler2D vbufferTex;

// Engel's barycentric coord partial derivs function. Follows equation from [Schied][Dachsbacher]
// Computes the partial derivatives of point's barycentric coordinates from the projected screen space vertices
SDerivativesOutput ComputePartialDerivatives(vec2 v[3])
{
	SDerivativesOutput derivatives;
	float d = 1.0 / determinant(mat2(v[2] - v[1], v[0] - v[1]));
	derivatives.dbDx = vec3(v[1].y - v[2].y, v[2].y - v[0].y, v[0].y - v[1].y) * d;
	derivatives.dbDy = vec3(v[2].x - v[1].x, v[0].x - v[2].x, v[1].x - v[0].x) * d;
	return derivatives;
}

// Interpolate 2D attributes using the partial derivatives and generates dx and dy for texture sampling.
vec2 Interpolate2DAttributes(mat3x2 attributes, vec3 dbDx, vec3 dbDy, vec2 d)
{
	vec3 attr0 = vec3(attributes[0].x, attributes[1].x, attributes[2].x);
	vec3 attr1 = vec3(attributes[0].y, attributes[1].y, attributes[2].y);
	vec2 attribute_x = vec2(dot(dbDx,attr0), dot(dbDx,attr1));
	vec2 attribute_y = vec2(dot(dbDy,attr0), dot(dbDy,attr1));
	vec2 attribute_s = attributes[0];
	
	vec2 result = (attribute_s + d.x * attribute_x + d.y * attribute_y);
	return result;
}

void main()
{
	vec2 drawID_PrimitiveID = texture(vbufferTex,texCoord).rg;
    uint drawID = uint(drawID_PrimitiveID.x);
    uint primitiveID = uint(drawID_PrimitiveID.y);

    SObjectData objData = objectBuffer.objects[drawID];

    uint meshIndex = objData.meshIndex;
    SVertex v0 = Vertices[meshIndex].vertices[primitiveID * 3 + 0];
    SVertex v1 = Vertices[meshIndex].vertices[primitiveID * 3 + 1];
    SVertex v2 = Vertices[meshIndex].vertices[primitiveID * 3 + 2];

    vec3 v0Pos = v0.positionXYZ_normalX.xyz;
    vec3 v1Pos = v1.positionXYZ_normalX.xyz;
    vec3 v2Pos = v2.positionXYZ_normalX.xyz;

    mat4 mvp = cameraData.viewProj * objData.model;

    // Transform positions to clip space
    vec4 clipPos0 = mvp * vec4(v0Pos, 1);
    vec4 clipPos1 = mvp * vec4(v1Pos, 1);
    vec4 clipPos2 = mvp * vec4(v2Pos, 1);

    clipPos0 /= clipPos0.w;
    clipPos1 /= clipPos1.w;
    clipPos1 /= clipPos1.w;
    vec2 screenPositions[3] = { clipPos0.xy, clipPos1.xy, clipPos2.xy };

    // Get barycentric coordinates for attribute interpolation
	SDerivativesOutput derivatives = ComputePartialDerivatives(screenPositions);

	// Interpolate texture coordinates
    mat3x2 triTexCoords =
    {
        vec2 (v0.normalYZ_texCoordUV.zw),
        vec2 (v1.normalYZ_texCoordUV.zw),
        vec2 (v2.normalYZ_texCoordUV.zw)
    };

	// Get delta vector that describes current screen point relative to vertex 0
	vec2 delta = texCoord + -screenPositions[0];

    vec2 interpTexCoords = Interpolate2DAttributes(triTexCoords, derivatives.dbDx, derivatives.dbDy, delta);

    vec3 diffuse = texture(texSet[objData.diffuseTexIndex], interpTexCoords).xyz;

	outFragColor = vec4(diffuse,1.0f);
}