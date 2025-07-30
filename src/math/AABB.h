#pragma once
#include <glm/glm.hpp>

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;

    bool intersects(const AABB &other) const
    {
        return (max.x > other.min.x && min.x < other.max.x) &&
               (max.y > other.min.y && min.y < other.max.y) &&
               (max.z > other.min.z && min.z < other.max.z);
    }
};