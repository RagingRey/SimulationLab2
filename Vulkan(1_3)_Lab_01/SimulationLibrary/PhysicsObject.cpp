#include "PhysicsObject.h"

void PhysicsObject::SetPosition(const glm::vec3& position) {
    m_Transform[3] = glm::vec4(position, 1.0f);
}

void PhysicsObject::SetOrientation(const glm::quat& orientation) {
    glm::vec3 position = GetPosition();
    m_Transform = glm::mat4_cast(orientation);
    m_Transform[3] = glm::vec4(position, 1.0f);
}

void PhysicsObject::SetOrientationEuler(const glm::vec3& eulerRadians) {
    SetOrientation(glm::quat(eulerRadians));
}