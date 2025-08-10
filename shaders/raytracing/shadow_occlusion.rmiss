#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT uint occluded;

void main()
{
    occluded = 0u;
}
