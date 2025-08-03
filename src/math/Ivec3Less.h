#pragma once
#include <glm/glm.hpp>
#include <functional>

struct ivec3_less
{
    bool operator()(const glm::ivec3 &a,
                    const glm::ivec3 &b) const noexcept
    {
        if (a.x != b.x)
            return a.x < b.x;
        if (a.y != b.y)
            return a.y < b.y;
        return a.z < b.z;
    }
};

struct ivec3_hasher
{
    std::size_t operator()(const glm::ivec3 &v) const noexcept
    {

        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};