#pragma once
#include <glm/glm.hpp>

struct ivec3_less {
    bool operator()(const glm::ivec3& a,
                    const glm::ivec3& b) const noexcept
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};
