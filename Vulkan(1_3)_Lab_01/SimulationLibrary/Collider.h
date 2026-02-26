#pragma once
#include <glm/glm.hpp>

class Collider {
public:
    enum class Type {
        Sphere,
        Plane
    };

    virtual ~Collider() = default;
    virtual Type GetType() const = 0;
    virtual void SyncFromTransform(const glm::mat4& transform) = 0;
};

class SphereCollider : public Collider {
public:
    explicit SphereCollider(float radius = 0.5f);

    Type GetType() const override { return Type::Sphere; }
    void SyncFromTransform(const glm::mat4& transform) override;

    float GetRadius() const { return m_Radius; }
    void SetRadius(float radius) { m_Radius = radius; }
    const glm::vec3& GetCenter() const { return m_Center; }

private:
    glm::vec3 m_Center{0.0f};
    float m_Radius = 0.5f;
};

class PlaneCollider : public Collider {
public:
    explicit PlaneCollider(const glm::vec3& normal = {0.0f, 1.0f, 0.0f});

    Type GetType() const override { return Type::Plane; }
    void SyncFromTransform(const glm::mat4& transform) override;

    const glm::vec3& GetNormal() const { return m_Normal; }
    const glm::vec3& GetPoint() const { return m_Point; }
    float DistanceToPoint(const glm::vec3& point) const;

private:
    glm::vec3 m_Normal{0.0f, 1.0f, 0.0f};
    glm::vec3 m_Point{0.0f};
};