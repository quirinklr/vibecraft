#pragma once
#include <glm/glm.hpp>
#include <cstdint>

static constexpr float ATLAS_INV_SIZE = 1.0f / 16.0f;

inline glm::vec2 atlasUV(int texture_coord, int corner) {
    int x = texture_coord % 16;
    int y = texture_coord / 16;
    float u = (x + (corner == 1 || corner == 2)) * ATLAS_INV_SIZE;
    float v = (y + (corner >= 2)) * ATLAS_INV_SIZE;
    return {u, v};
}
