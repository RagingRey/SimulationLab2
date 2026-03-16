#include "PhysicsObject.h"

void PhysicsObject::SetTransform(const glm::mat4& transform) {
    m_Transform = transform;
    SyncCollider();
}

void PhysicsObject::SetPosition(const glm::vec3& position) {
    m_Transform[3] = glm::vec4(position, 1.0f);
    SyncCollider();
}

void PhysicsObject::SetOrientation(const glm::quat& orientation) {
    glm::vec3 position = GetPosition();
    m_Transform = glm::mat4_cast(glm::normalize(orientation));
    m_Transform[3] = glm::vec4(position, 1.0f);
    SyncCollider();
}

void PhysicsObject::SetOrientationEuler(const glm::vec3& eulerRadians) {
    SetOrientation(glm::quat(eulerRadians));
}

void PhysicsObject::ApplyAngularDisplacementEuler(const glm::vec3& deltaRadians) {
    const glm::quat current = GetOrientation();
    const glm::quat delta = glm::quat(deltaRadians);
    SetOrientation(current * delta);
}

void PhysicsObject::IntegrateAngularVelocity(float deltaTime) {
    if (glm::length2(m_AngularVelocity) <= 0.0f) {
        return;
    }
    ApplyAngularDisplacementEuler(m_AngularVelocity * deltaTime);
}

void PhysicsObject::SetRadius(float radius) {
    m_Radius = radius;
    if (auto* sphere = GetColliderAs<SphereCollider>()) {
        sphere->SetRadius(radius);
    }
}

void PhysicsObject::SetMass(float mass) {
    if (mass <= 0.0f) {
        m_Mass = 0.0f;
        m_InverseMass = 0.0f;
        return;
    }
    m_Mass = mass;
    m_InverseMass = 1.0f / mass;
}

void PhysicsObject::AddForce(const glm::vec3& force) {
    if (m_InverseMass == 0.0f) {
        return;
    }
    m_ForceAccumulator += force;
}

void PhysicsObject::ClearForces() {
    m_ForceAccumulator = glm::vec3(0.0f);
}

void PhysicsObject::SetCollider(std::unique_ptr<Collider> collider) {
    m_Collider = std::move(collider);
    SyncCollider();
}

void PhysicsObject::SyncCollider() {
    if (m_Collider) {
        m_Collider->SyncFromTransform(m_Transform);
    }
}

void PhysicsObject::Update(float deltaTime, float gravity, IntegrationMethod method) {
    IntegrateAngularVelocity(deltaTime);

    if (m_InverseMass == 0.0f) {
        ClearForces();
        return;
    }

    AddForce({ 0.0f, gravity * m_Mass, 0.0f });

    glm::vec3 position = GetPosition();
    glm::vec3 acceleration = m_ForceAccumulator * m_InverseMass;

    switch (method) {
        case IntegrationMethod::ExplicitEuler:
            position += m_Velocity * deltaTime;
            m_Velocity += acceleration * deltaTime;
            break;

        case IntegrationMethod::SemiImplicitEuler:
        default:
            m_Velocity += acceleration * deltaTime;
            position += m_Velocity * deltaTime;
            break;
    }

    SetPosition(position);
    ClearForces();
}