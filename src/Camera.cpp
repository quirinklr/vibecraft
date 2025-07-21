#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

void Camera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {
    // LookAt-Matrix für eine "Ziel"-basierte Kamera
    const glm::vec3 w{glm::normalize(direction)};
    const glm::vec3 u{glm::normalize(glm::cross(w, up))};
    const glm::vec3 v{glm::cross(w, u)};

    m_ViewMatrix = glm::mat4{1.f};
    m_ViewMatrix[0][0] = u.x;
    m_ViewMatrix[1][0] = u.y;
    m_ViewMatrix[2][0] = u.z;
    m_ViewMatrix[0][1] = v.x;
    m_ViewMatrix[1][1] = v.y;
    m_ViewMatrix[2][1] = v.z;
    m_ViewMatrix[0][2] = w.x;
    m_ViewMatrix[1][2] = w.y;
    m_ViewMatrix[2][2] = w.z;
    m_ViewMatrix[3][0] = -glm::dot(u, position);
    m_ViewMatrix[3][1] = -glm::dot(v, position);
    m_ViewMatrix[3][2] = -glm::dot(w, position);
}

void Camera::setPerspectiveProjection(float fovy, float aspect, float near, float far) {
    m_ProjectionMatrix = glm::perspective(fovy, aspect, near, far);
    m_ProjectionMatrix[1][1] *= -1; // Y-Flip für Vulkan
}