#pragma once
#include <glm/glm.hpp>

struct UniformBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(4) float time;
};
