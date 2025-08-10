#version 460
#extension GL_EXT_ray_tracing : require

struct PrimaryPayload {
    vec3  hitPos;
    float t;
    uint  hit;
};
layout(location = 0) rayPayloadInEXT PrimaryPayload prd;

void main()
{
    float t  = gl_HitTEXT;
    vec3  ro = gl_WorldRayOriginEXT;
    vec3  rd = gl_WorldRayDirectionEXT;

    prd.t      = t;
    prd.hitPos = ro + rd * t;
    prd.hit    = 1u;
}
