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

glm::mat3 PhysicsObject::GetWorldInverseInertiaTensor() const {
    if (m_InverseMass == 0.0f) return glm::mat3(0.0f); // Static objects don't rotate

    // Convert orientation quaternion to a 3x3 rotation matrix
    glm::mat3 R = glm::mat3_cast(GetOrientation());

    // I_world^-1 = R * I_local^-1 * R^T
    return R * m_LocalInverseInertiaTensor * glm::transpose(R);
}

void PhysicsObject::SetSphereInertia(float mass, float radius) {
    if (mass <= 0.0f) {
        m_LocalInverseInertiaTensor = glm::mat3(0.0f);
        return;
    }
    // Solid sphere inertia: I = 2/5 * m * r^2
    float i = (2.0f / 5.0f) * mass * (radius * radius);
    m_LocalInverseInertiaTensor = glm::inverse(glm::mat3(i));
}

void PhysicsObject::SetCuboidInertia(float mass, const glm::vec3& halfExtents) {
    if (mass <= 0.0f) {
        m_LocalInverseInertiaTensor = glm::mat3(0.0f);
        return;
    }
    // Solid cuboid inertia: I = 1/12 * m * (w^2 + h^2)
    glm::vec3 size = halfExtents * 2.0f;
    glm::mat3 inertia(0.0f);
    inertia[0][0] = (1.0f / 12.0f) * mass * (size.y * size.y + size.z * size.z);
    inertia[1][1] = (1.0f / 12.0f) * mass * (size.x * size.x + size.z * size.z);
    inertia[2][2] = (1.0f / 12.0f) * mass * (size.x * size.x + size.y * size.y);

    m_LocalInverseInertiaTensor = glm::inverse(inertia);
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

void PhysicsObject::ResolveCollision(PhysicsObject* objA, PhysicsObject* objB, const SimCollision::Contact& contact)
{
    float invMassA = objA->GetInverseMass();
    float invMassB = objB->GetInverseMass();

    // If both are static/infinite mass, do nothing
    if (invMassA == 0.0f && invMassB == 0.0f) return;

    // 1. Positional Correction (Prevents sinking)
    const float percent = 0.8f; // Usually 0.2 to 0.8
    const float slop = 0.01f;   // Penetration allowance
    glm::vec3 correction = (std::max(contact.penetration - slop, 0.0f) / (invMassA + invMassB)) * percent * contact.normal;

    if (invMassA > 0.0f) objA->SetPosition(objA->GetPosition() - correction * invMassA);
    if (invMassB > 0.0f) objB->SetPosition(objB->GetPosition() + correction * invMassB);

    // 2. Impulse Resolution (The Bouncing)
    // We assume the contact point is exactly between the two objects for spheres/simple shapes.
    // In a fully robust engine, SimCollision::Contact would return the exact world-space contact point.
    glm::vec3 contactPoint = objA->GetPosition() + (contact.normal * objA->GetRadius());

    glm::vec3 rA = contactPoint - objA->GetPosition();
    glm::vec3 rB = contactPoint - objB->GetPosition();

    glm::vec3 velA = objA->GetVelocity() + glm::cross(objA->GetAngularVelocity(), rA);
    glm::vec3 velB = objB->GetVelocity() + glm::cross(objB->GetAngularVelocity(), rB);
    glm::vec3 relativeVel = velB - velA;

    float velAlongNormal = glm::dot(relativeVel, contact.normal);
    if (velAlongNormal > 0.0f) return; // Objects are already moving apart

    // Combined restitution (bounciness)
    float e = std::min(objA->GetRestitution(), objB->GetRestitution());

    glm::mat3 invInertiaA = objA->GetWorldInverseInertiaTensor();
    glm::mat3 invInertiaB = objB->GetWorldInverseInertiaTensor();

    // Calculate rotational resistance
    glm::vec3 angularFactorA = glm::cross(invInertiaA * glm::cross(rA, contact.normal), rA);
    glm::vec3 angularFactorB = glm::cross(invInertiaB * glm::cross(rB, contact.normal), rB);
    float angularResistance = glm::dot(angularFactorA + angularFactorB, contact.normal);

    // Calculate impulse magnitude (j)
    float j = -(1.0f + e) * velAlongNormal;
    j /= (invMassA + invMassB + angularResistance);

    // Apply impulse
    glm::vec3 impulse = j * contact.normal;

    if (invMassA > 0.0f) {
        objA->SetVelocity(objA->GetVelocity() - impulse * invMassA);
        objA->SetAngularVelocity(objA->GetAngularVelocity() - invInertiaA * glm::cross(rA, impulse));
    }
    if (invMassB > 0.0f) {
        objB->SetVelocity(objB->GetVelocity() + impulse * invMassB);
        objB->SetAngularVelocity(objB->GetAngularVelocity() + invInertiaB * glm::cross(rB, impulse));
    }
}
