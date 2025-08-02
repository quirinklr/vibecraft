#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float shadowResult;
layout(location = 1) rayPayloadEXT bool isShadowed;

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

    
    vec3 f = fract(hitPos);
    vec3 inv_f = 1.0 - f;
    vec3 f_dist = min(f, inv_f);
    float min_dist = min(f_dist.x, min(f_dist.y, f_dist.z));
    
    vec3 normal = vec3(0.0);
    if (min_dist == f_dist.x) {
        normal.x = step(inv_f.x, f.x) * 2.0 - 1.0;
    } else if (min_dist == f_dist.y) {
        normal.y = step(inv_f.y, f.y) * 2.0 - 1.0;
    } else {
        normal.z = step(inv_f.z, f.z) * 2.0 - 1.0;
    }
    
    
    normal = -sign(dot(gl_WorldRayDirectionEXT, normal)) * normal;


    vec3 origin = hitPos + normal * 0.001;
    vec3 direction = pc.lightDirection;

    isShadowed = false;

    traceRayEXT(topLevelAS,
                gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
                0xFF,
                1,
                0,
                1,
                origin,
                0.001,
                direction,
                10000.0,
                1);

    shadowResult = isShadowed ? 0.0 : 1.0;
}