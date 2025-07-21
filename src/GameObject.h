#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

struct GameObject
{
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 scale{1.f};

private:
    mutable glm::mat4 cachedModelMatrix{1.f};
    mutable bool isDirty = true;

public:
    void setPosition(const glm::vec3 &pos);
    const glm::mat4 &getModelMatrix() const;
};