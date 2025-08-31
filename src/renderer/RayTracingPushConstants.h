#pragma once
#include <glm/glm.hpp>

struct RayTracePushConstants
{
    glm::vec3 camPos;
    float fovYTan;
    glm::vec3 camRight;
    float pad0 = 0.0f;
    glm::vec3 camUp;
    float pad1 = 0.0f;
    glm::vec3 camForward;
    float pad2 = 0.0f;
    glm::vec3 lightDir;
    float pad3 = 0.0f;
    glm::vec4 shadowParams;   // x: shadowMode (0 off,1 soft), y: angular radius, z: samples, w: denoise mode
    glm::vec4 renderParams;   // x: reflectionMode (0 off..3 high), y: nightBrightness, z: shadowMinVisibility, w: GI mode
};
