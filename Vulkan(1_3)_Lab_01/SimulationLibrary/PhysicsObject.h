#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

class PhysicsObject {
public:
    PhysicsObject() = default;
    explicit PhysicsObject(const glm::mat4& transform) : m_Transform(transform) {}

    const glm::mat4& GetTransform() const { return m_Transform; }
    void SetTransform(const glm::mat4& transform) { m_Transform = transform; }

    glm::vec3 GetPosition() const { return glm::vec3(m_Transform[3]); }
    void SetPosition(const glm::vec3& position);

    glm::quat GetOrientation() const { return glm::quat_cast(m_Transform); }
    void SetOrientation(const glm::quat& orientation);
    void SetOrientationEuler(const glm::vec3& eulerRadians);

private:
    glm::mat4 m_Transform{1.0f};
};