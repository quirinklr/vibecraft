#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

void Camera::updateFrustum()
{
    const glm::mat4 viewProj = m_ProjectionMatrix * m_ViewMatrix;
    m_Frustum.update(viewProj);
}

void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up)
{

    m_ViewMatrix = glm::lookAt(position, position + direction, up);

    updateFrustum();
}

void Camera::setPerspectiveProjection(float fovy, float aspect, float near, float far)
{
    m_ProjectionMatrix = glm::perspective(fovy, aspect, near, far);

    m_ProjectionMatrix[1][1] *= -1;

    updateFrustum();
}