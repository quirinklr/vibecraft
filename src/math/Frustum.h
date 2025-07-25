#pragma once
#include "AABB.h"
#include <glm/glm.hpp>
#include <array>

struct Plane
{
    glm::vec3 normal{0.f, 1.f, 0.f};
    float distance = 0.f;

    float getSignedDistance(const glm::vec3 &point) const;
};

class Frustum
{
public:
    void update(const glm::mat4 &viewProjMatrix);
    bool intersects(const AABB &aabb) const;

private:
    std::array<Plane, 6> planes;
};