#extension GL_EXT_nonuniform_qualifier:enable
#extension GL_EXT_shader_explicit_arithmetic_types: enable
#extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
#extension GL_EXT_shader_explicit_arithmetic_types_float16: enable


struct BarycentricDeriv {
    vec3 m_lambda;
    vec3 m_ddx;
    vec3 m_ddy;
};

BarycentricDeriv CalcFullBary(
    vec4 pt0,
    vec4 pt1,
    vec4 pt2,
    vec2 pixelNdc,
    vec2 winSize
) {
    // EDIT: got rid of the cast.
    BarycentricDeriv ret;

    vec3 invW = 1. / vec3(pt0.w, pt1.w, pt2.w);

    vec2 ndc0 = pt0.xy * invW.x;
    vec2 ndc1 = pt1.xy * invW.y;
    vec2 ndc2 = pt2.xy * invW.z;

    float invDet = 1. /  determinant(mat2x2(ndc2 - ndc1, ndc0 - ndc1));
    ret.m_ddx = vec3(ndc1.y - ndc2.y, ndc2.y - ndc0.y, ndc0.y - ndc1.y)
        * invDet * invW;
    ret.m_ddy = vec3(ndc2.x - ndc1.x, ndc0.x - ndc2.x, ndc1.x - ndc0.x)
        * invDet * invW;
    float ddxSum = dot(ret.m_ddx, vec3(1, 1, 1));
    float ddySum = dot(ret.m_ddy, vec3(1, 1, 1));

    vec2 deltaVec = pixelNdc - ndc0;
    float interpInvW = invW.x + deltaVec.x * ddxSum + deltaVec.y * ddySum;
    float interpW = 1. / interpInvW;

    ret.m_lambda.x = interpW
        * (invW[0] + deltaVec.x * ret.m_ddx.x + deltaVec.y * ret.m_ddy.x);
    ret.m_lambda.y =
        interpW * (0.0f + deltaVec.x * ret.m_ddx.y + deltaVec.y * ret.m_ddy.y);
    ret.m_lambda.z =
        interpW * (0.0f + deltaVec.x * ret.m_ddx.z + deltaVec.y * ret.m_ddy.z);

    ret.m_ddx *= (2.0f / winSize.x);
    ret.m_ddy *= (2.0f / winSize.y);
    ddxSum *= (2.0f / winSize.x);
    ddySum *= (2.0f / winSize.y);

    ret.m_ddy *= -1.0f;
    ddySum *= -1.0f;

    float interpW_ddx = 1.0f / (interpInvW + ddxSum);
    float interpW_ddy = 1.0f / (interpInvW + ddySum);

    ret.m_ddx =
        interpW_ddx * (ret.m_lambda * interpInvW + ret.m_ddx) - ret.m_lambda;
    ret.m_ddy =
        interpW_ddy * (ret.m_lambda * interpInvW + ret.m_ddy) - ret.m_lambda;

    return ret;
}

vec3
InterpolateWithDeriv(BarycentricDeriv deriv, float v0, float v1, float v2) {
    vec3 mergedV = vec3(v0, v1, v2);
    vec3 ret;
    ret.x = dot(mergedV, deriv.m_lambda);
    ret.y = dot(mergedV, deriv.m_ddx);
    ret.z = dot(mergedV, deriv.m_ddy);
    return ret;
}

// Helper structs.

struct InterpolatedVector_vec3 {
    vec3 value;
    vec3 dx;
    vec3 dy;
};

struct InterpolatedVector_vec2 {
    vec2 value;
    vec2 dx;
    vec2 dy;
};

InterpolatedVector_vec2
interpolate(BarycentricDeriv deriv, vec2 v0, vec2 v1, vec2 v2) {
    InterpolatedVector_vec2 interp;
    vec3 x = InterpolateWithDeriv(deriv, v0.x, v1.x, v2.x);
    interp.value.x = x.x;
    interp.dx.x = x.y;
    interp.dy.x = x.z;
    vec3 y = InterpolateWithDeriv(deriv, v0.y, v1.y, v2.y);
    interp.value.y = y.x;
    interp.dx.y = y.y;
    interp.dy.y = y.z;
    return interp;
};

InterpolatedVector_vec3
interpolate(BarycentricDeriv deriv, vec3 v0, vec3 v1, vec3 v2) {
    InterpolatedVector_vec3 interp;
    vec3 x = InterpolateWithDeriv(deriv, v0.x, v1.x, v2.x);
    interp.value.x = x.x;
    interp.dx.x = x.y;
    interp.dy.x = x.z;
    vec3 y = InterpolateWithDeriv(deriv, v0.y, v1.y, v2.y);
    interp.value.y = y.x;
    interp.dx.y = y.y;
    interp.dy.y = y.z;
    vec3 z = InterpolateWithDeriv(deriv, v0.z, v1.z, v2.z);
    interp.value.z = z.x;
    interp.dx.z = z.y;
    interp.dy.z = z.z;
    return interp;
};

uvec2 packVBuffer(uint objectID, uint meshletsID, uint primitiveID)
{
    uint rchanel = objectID << 16 | meshletsID;
    return uvec2(rchanel, primitiveID);
}   

uvec3 unpackVBuffer(uvec2 vbuffer)
{
    uint objectID = (vbuffer.x >> 16);
    uint meshletsID = vbuffer.x & 0xFFFF;
    uint primitiveID = vbuffer.y;
    return uvec3(objectID, meshletsID, primitiveID);
}
