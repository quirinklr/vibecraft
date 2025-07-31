#version 460 core
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float shadowPayload;

void main()
{
    shadowPayload = 1.0; 
}