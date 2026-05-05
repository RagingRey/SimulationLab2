#include "Collider.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

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

BoxCollider::BoxCollider(const glm::vec3& halfExtents)
    : m_HalfExtents(glm::max(halfExtents, glm::vec3(0.01f))) {}

void BoxCollider::SyncFromTransform(const glm::mat4& transform)
{
    m_Center = glm::vec3(transform[3]);

    // Extract rotation (ignore scaling as best-effort by normalizing columns)
    glm::vec3 x = glm::vec3(transform[0]);
    glm::vec3 y = glm::vec3(transform[1]);
    glm::vec3 z = glm::vec3(transform[2]);

    auto safeNorm = [](const glm::vec3& v, const glm::vec3& fallback)
    {
        const float len2 = glm::dot(v, v);
        if (len2 <= 1e-8f) return fallback;
        return v * (1.0f / std::sqrt(len2));
    };

    x = safeNorm(x, { 1,0,0 });
    y = y - x * glm::dot(y, x);
    y = safeNorm(y, { 0,1,0 });
    z = glm::cross(x, y);
    z = safeNorm(z, { 0,0,1 });

    m_Orientation[0] = x;
    m_Orientation[1] = y;
    m_Orientation[2] = z;
}