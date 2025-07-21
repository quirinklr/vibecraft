#include "GameObject.h"

void GameObject::setPosition(const glm::vec3& pos) {
    position = pos;
    isDirty = true;
}

const glm::mat4& GameObject::getModelMatrix() const {
    if (isDirty) {
        glm::mat4 transform = glm::translate(glm::mat4(1.f), position);
        transform = transform * glm::eulerAngleYXZ(rotation.y, rotation.x, rotation.z);
        cachedModelMatrix = glm::scale(transform, scale);
        isDirty = false;
    }
    return cachedModelMatrix;
}