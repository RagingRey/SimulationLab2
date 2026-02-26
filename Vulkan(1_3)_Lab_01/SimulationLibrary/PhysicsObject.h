#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include "Collider.h"
#include "IntegrationMethod.h"

class PhysicsObject {
public:
    PhysicsObject() = default;
    explicit PhysicsObject(const glm::mat4& transform) : m_Transform(transform) {}

    const glm::mat4& GetTransform() const { return m_Transform; }
    void SetTransform(const glm::mat4& transform);

    glm::vec3 GetPosition() const { return glm::vec3(m_Transform[3]); }
    void SetPosition(const glm::vec3& position);

    glm::quat GetOrientation() const { return glm::quat_cast(m_Transform); }
    void SetOrientation(const glm::quat& orientation);
    void SetOrientationEuler(const glm::vec3& eulerRadians);

    const glm::vec3& GetVelocity() const { return m_Velocity; }
    void SetVelocity(const glm::vec3& velocity) { m_Velocity = velocity; }

    float GetRadius() const { return m_Radius; }
    void SetRadius(float radius);

    float GetRestitution() const { return m_Restitution; }
    void SetRestitution(float restitution) { m_Restitution = restitution; }

    float GetMass() const { return m_Mass; }
    float GetInverseMass() const { return m_InverseMass; }
    void SetMass(float mass);

    void AddForce(const glm::vec3& force);
    void ClearForces();

    void SetCollider(std::unique_ptr<Collider> collider);
    Collider* GetCollider() const { return m_Collider.get(); }

    template <typename T>
    T* GetColliderAs() const { return dynamic_cast<T*>(m_Collider.get()); }

    void Update(float deltaTime, float gravity, IntegrationMethod method);

private:
    void SyncCollider();

    glm::mat4 m_Transform{1.0f};
    glm::vec3 m_Velocity{0.0f};
    float m_Radius = 0.5f;
    float m_Restitution = 0.8f;

    float m_Mass = 1.0f;
    float m_InverseMass = 1.0f;
    glm::vec3 m_ForceAccumulator{0.0f};

    std::unique_ptr<Collider> m_Collider;
};