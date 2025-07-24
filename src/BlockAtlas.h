#pragma once
#include <glm/glm.hpp>
#include <cstdint>

using BlockID = uint8_t;

static constexpr float ATLAS_INV_SIZE = 1.0f / 16.0f;

inline glm::vec2 atlasUV(BlockID id, int corner) {
    int x = id % 16;
    int y = id / 16;
    float u = (x + (corner == 1 || corner == 2)) * ATLAS_INV_SIZE;
    float v = (y + (corner >= 2)) * ATLAS_INV_SIZE;
    return {u, v};
}
