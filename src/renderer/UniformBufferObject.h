#pragma once
#include <glm/glm.hpp>

struct LightUbo
{
    alignas(16) glm::vec3 lightDirection;
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 cameraPos;
    alignas(16) glm::vec3 skyColor;
    alignas(4) float time;
    alignas(4) int isUnderwater;
    alignas(4) uint32_t flags;
};