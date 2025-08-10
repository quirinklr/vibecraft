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
    prd.hit = 0u;
    prd.t   = 1e30;
}
