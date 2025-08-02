#version 460
#extension GL_EXT_ray_tracing : enable





layout(location = 0) rayPayloadInEXT vec3 finalColor;


layout(location = 1) rayPayloadEXT bool isOccluded;


layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;


layout(push_constant) uniform PushConstants {
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPos;
    vec3 lightDirection;
} pc;

void main() {
    
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    
    isOccluded = false;

    
    
    vec3 direction = normalize(pc.lightDirection);
    vec3 origin = hitPos + direction * 0.001;

    
    traceRayEXT(
        topLevelAS,
        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
        0xFF,      
        0,         
        0,         
        1,         
        origin,    
        0.001,     
        direction, 
        10000.0,   
        1          
    );

    
    
    finalColor = vec3(isOccluded ? 0.0 : 1.0, 0.0, 0.0);
}