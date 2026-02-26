#include "Collider.h"
#include <glm/gtc/matrix_transform.hpp>

SphereCollider::SphereCollider(float radius)
    : m_Radius(radius) {}

void SphereCollider::SyncFromTransform(const glm::mat4& transform) {
    m_Center = glm::vec3(transform[3]);
}

PlaneCollider::PlaneCollider(const glm::vec3& normal)
    : m_Normal(glm::normalize(normal)) {}

void PlaneCollider::SyncFromTransform(const glm::mat4& transform) {
    m_Point = glm::vec3(transform[3]);
    glm::vec3 up = glm::normalize(glm::vec3(transform * glm::vec4(0, 1, 0, 0)));
    if (glm::length(up) > 0.0f) {
        m_Normal = up;
    }
}

float PlaneCollider::DistanceToPoint(const glm::vec3& point) const {
    return glm::dot(m_Normal, point - m_Point);
}