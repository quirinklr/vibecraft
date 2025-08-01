#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float shadowPayload;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;

layout(push_constant) uniform Constants {
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPos;
    vec3 lightDirection;
} pc;

void main()
{
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 shadowRayOrigin = hitPos + pc.lightDirection * 0.01;
    
    traceRayEXT(topLevelAS,
                gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
                0xFF, 0, 0, 1, 
                shadowRayOrigin, 0.0, pc.lightDirection, 10000.0,
                0); 
}