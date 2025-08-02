#pragma once
#include <glm/glm.hpp>

struct RayTracePushConstants
{
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    alignas(16) glm::vec3 cameraPos;
    alignas(16) glm::vec3 lightDirection;
};