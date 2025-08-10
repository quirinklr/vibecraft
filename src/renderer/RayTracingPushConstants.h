#pragma once
#include <glm/glm.hpp>

struct RayTracePushConstants
{

    glm::mat4 invViewProj;

    glm::vec3 cameraPos;
    float _pad0 = 0.0f;

    glm::vec3 sunDirWS;
    float tMin = 0.001f;

    float tMax = 1e16f;
    glm::vec3 _pad1 = glm::vec3(0.0f);
};
