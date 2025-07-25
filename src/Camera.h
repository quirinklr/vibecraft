#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "math/Frustum.h"

class Camera
{
public:
    void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, 1.f, 0.f});
    void setPerspectiveProjection(float fovy, float aspect, float near, float far);

    const glm::mat4 &getProjectionMatrix() const { return m_ProjectionMatrix; }
    const glm::mat4 &getViewMatrix() const { return m_ViewMatrix; }
    const Frustum &getFrustum() const { return m_Frustum; }

private:
    void updateFrustum();

    glm::mat4 m_ProjectionMatrix{1.f};
    glm::mat4 m_ViewMatrix{1.f};
    Frustum m_Frustum;
};