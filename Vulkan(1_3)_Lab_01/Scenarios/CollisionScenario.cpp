#include "CollisionScenario.h"
#include "../SimulationLibrary/CollisionUtil.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

static Mesh BuildCuboidMesh(const glm::vec3& halfExtents, const glm::vec3& color)
{
    const float hx = halfExtents.x;
    const float hy = halfExtents.y;
    const float hz = halfExtents.z;

    Mesh mesh;
    mesh.vertices = {
        // +X
        {{ hx,-hy,-hz}, color, { 1, 0, 0}}, {{ hx, hy,-hz}, color, { 1, 0, 0}},
        {{ hx, hy, hz}, color, { 1, 0, 0}}, {{ hx,-hy, hz}, color, { 1, 0, 0}},
        // -X
        {{-hx,-hy, hz}, color, {-1, 0, 0}}, {{-hx, hy, hz}, color, {-1, 0, 0}},
        {{-hx, hy,-hz}, color, {-1, 0, 0}}, {{-hx,-hy,-hz}, color, {-1, 0, 0}},
        // +Y
        {{-hx, hy,-hz}, color, { 0, 1, 0}}, {{-hx, hy, hz}, color, { 0, 1, 0}},
        {{ hx, hy, hz}, color, { 0, 1, 0}}, {{ hx, hy,-hz}, color, { 0, 1, 0}},
        // -Y
        {{-hx,-hy, hz}, color, { 0,-1, 0}}, {{-hx,-hy,-hz}, color, { 0,-1, 0}},
        {{ hx,-hy,-hz}, color, { 0,-1, 0}}, {{ hx,-hy, hz}, color, { 0,-1, 0}},
        // +Z
        {{-hx,-hy, hz}, color, { 0, 0, 1}}, {{ hx,-hy, hz}, color, { 0, 0, 1}},
        {{ hx, hy, hz}, color, { 0, 0, 1}}, {{-hx, hy, hz}, color, { 0, 0, 1}},
        // -Z
        {{ hx,-hy,-hz}, color, { 0, 0,-1}}, {{-hx,-hy,-hz}, color, { 0, 0,-1}},
        {{-hx, hy,-hz}, color, { 0, 0,-1}}, {{ hx, hy,-hz}, color, { 0, 0,-1}},
    };

    for (uint32_t f = 0; f < 6; ++f) {
        const uint32_t b = f * 4;
        mesh.indices.push_back(b + 0); mesh.indices.push_back(b + 1); mesh.indices.push_back(b + 2);
        mesh.indices.push_back(b + 0); mesh.indices.push_back(b + 2); mesh.indices.push_back(b + 3);
    }

    return mesh;
}

void CollisionScenario::ClearScene() {
    for (auto& sphere : m_Spheres) m_App->DestroyMeshBuffers(sphere.buffers);
    for (auto& plane : m_Planes) m_App->DestroyMeshBuffers(plane.buffers);
    for (auto& box : m_Boxes) m_App->DestroyMeshBuffers(box.buffers);
    m_Spheres.clear();
    m_Planes.clear();
    m_Boxes.clear();
}

void CollisionScenario::UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position) {
    glm::mat4 transform = glm::mat4_cast(plane.orientation);
    transform[3] = glm::vec4(position, 1.0f);
    plane.body.SetTransform(transform);
}

void CollisionScenario::AddSphere(const glm::vec3& position, const glm::vec3& velocity, float radius, float mass, const glm::vec3& color) {
    SphereInstance instance{};
    instance.color = color;

    instance.body.SetCollider(std::make_unique<SphereCollider>(radius));
    instance.body.SetPosition(position);
    instance.body.SetVelocity(velocity);
    instance.body.SetRadius(radius);
    instance.body.SetMass(mass);
    instance.body.SetRestitution(m_BounceRestitution);

    instance.mesh = MeshGenerator::GenerateSphere(radius, 32, 16, color);
    instance.buffers = m_App->UploadMesh(instance.mesh);
    m_Spheres.push_back(std::move(instance));
}

void CollisionScenario::AddPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec2& size, const glm::vec3& color) {
    PlaneInstance instance{};
    instance.size = size;
    instance.color = color;

    glm::vec3 n = glm::normalize(normal);
    instance.orientation = glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), n);

    instance.body.SetCollider(std::make_unique<PlaneCollider>(n));
    instance.body.SetMass(0.0f);
    UpdatePlaneTransform(instance, point);

    instance.mesh = MeshGenerator::GeneratePlane(size.x, size.y, 10, 10, color);
    instance.buffers = m_App->UploadMesh(instance.mesh);
    m_Planes.push_back(std::move(instance));
}

void CollisionScenario::AddBox(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& halfExtents, const glm::vec3& velocity, float mass, const glm::vec3& color)
{
    BoxInstance instance{};
    instance.color = color;
    instance.halfExtents = glm::max(halfExtents, glm::vec3(0.05f));

    instance.body.SetCollider(std::make_unique<BoxCollider>(instance.halfExtents));
    instance.body.SetPosition(position);
    instance.body.SetOrientation(orientation);
    instance.body.SetVelocity(velocity);
    instance.body.SetMass(mass);
    instance.body.SetRestitution(m_BounceRestitution);

    instance.mesh = BuildCuboidMesh(instance.halfExtents, color);
    instance.buffers = m_App->UploadMesh(instance.mesh);
    m_Boxes.push_back(std::move(instance));
}

void CollisionScenario::ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane) {
    auto* sphereCollider = sphere.body.GetColliderAs<SphereCollider>();
    if (!sphereCollider) return;

    const float distance = plane.DistanceToPoint(sphereCollider->GetCenter());
    const float radius = sphereCollider->GetRadius();

    if (distance < radius) {
        const float penetration = radius - distance;
        const glm::vec3 normal = plane.GetNormal();

        sphere.body.SetPosition(sphere.body.GetPosition() + normal * penetration);

        glm::vec3 velocity = sphere.body.GetVelocity();
        const float normalVelocity = glm::dot(velocity, normal);

        if (normalVelocity < 0.0f) {
            if (m_UseBounce) {
                const float restitution = sphere.body.GetRestitution();
                velocity = velocity - (1.0f + restitution) * normalVelocity * normal;
                sphere.body.SetVelocity(velocity);
            }
            else {
                sphere.body.SetVelocity(glm::vec3(0.0f));
            }
        }
    }
}

void CollisionScenario::ResolveSphereSphere(SphereInstance& a, SphereInstance& b) {
    auto* aCollider = a.body.GetColliderAs<SphereCollider>();
    auto* bCollider = b.body.GetColliderAs<SphereCollider>();
    if (!aCollider || !bCollider) return;

    const glm::vec3 delta = bCollider->GetCenter() - aCollider->GetCenter();
    const float distance = glm::length(delta);
    const float radiusSum = aCollider->GetRadius() + bCollider->GetRadius();
    if (distance <= 0.0f || distance >= radiusSum) return;

    const glm::vec3 normal = delta / distance;

    const float penetration = radiusSum - distance;
    const float invA = a.body.GetInverseMass();
    const float invB = b.body.GetInverseMass();
    const float invSum = invA + invB;
    if (invSum <= 0.0f) return;

    const glm::vec3 correction = normal * (penetration / invSum);
    a.body.SetPosition(a.body.GetPosition() - correction * invA);
    b.body.SetPosition(b.body.GetPosition() + correction * invB);

    glm::vec3 va = a.body.GetVelocity();
    glm::vec3 vb = b.body.GetVelocity();
    const glm::vec3 relativeVelocity = vb - va;
    const float velocityAlongNormal = glm::dot(relativeVelocity, normal);
    if (velocityAlongNormal > 0.0f) return;

    const float restitution = m_UseBounce ? m_BounceRestitution : 0.0f;
    const float j = -(1.0f + restitution) * velocityAlongNormal / invSum;
    const glm::vec3 impulse = j * normal;

    a.body.SetVelocity(va - impulse * invA);
    b.body.SetVelocity(vb + impulse * invB);
}

void CollisionScenario::ResolveSphereBox(SphereInstance& sphere, BoxInstance& box)
{
    auto* sCol = sphere.body.GetColliderAs<SphereCollider>();
    auto* bCol = box.body.GetColliderAs<BoxCollider>();
    if (!sCol || !bCol) return;

    SimCollision::OBB obb{};
    obb.center = bCol->GetCenter();
    obb.orientation = bCol->GetOrientation();
    obb.halfExtents = bCol->GetHalfExtents();

    SimCollision::Contact c{};
    if (!SimCollision::SphereVsOBB(sCol->GetCenter(), sCol->GetRadius(), obb, c)) return;

    // Push apart (treat normal as box->sphere, but we need A->B style response)
    const glm::vec3 n = c.normal;
    const float invS = sphere.body.GetInverseMass();
    const float invB = box.body.GetInverseMass();
    const float invSum = invS + invB;
    if (invSum <= 0.0f) return;

    const glm::vec3 corr = n * (c.penetration / invSum);
    box.body.SetPosition(box.body.GetPosition() - corr * invB);
    sphere.body.SetPosition(sphere.body.GetPosition() + corr * invS);

    // Impulse
    glm::vec3 vs = sphere.body.GetVelocity();
    glm::vec3 vb = box.body.GetVelocity();
    const glm::vec3 rel = vs - vb;
    const float velN = glm::dot(rel, n);
    if (velN > 0.0f) return;

    const float e = m_UseBounce ? m_BounceRestitution : 0.0f;
    const float j = -(1.0f + e) * velN / invSum;
    const glm::vec3 impulse = j * n;

    sphere.body.SetVelocity(vs + impulse * invS);
    box.body.SetVelocity(vb - impulse * invB);
}

void CollisionScenario::ResolveBoxPlane(BoxInstance& box, const PlaneCollider& plane)
{
    auto* bCol = box.body.GetColliderAs<BoxCollider>();
    if (!bCol) return;

    SimCollision::OBB obb{};
    obb.center = bCol->GetCenter();
    obb.orientation = bCol->GetOrientation();
    obb.halfExtents = bCol->GetHalfExtents();

    const glm::vec3 n = plane.GetNormal();
    const float dist = plane.DistanceToPoint(obb.center);
    const float r = SimCollision::SupportDistanceAlongNormal(obb, n);

    if (dist >= r) return;

    const float penetration = r - dist;
    box.body.SetPosition(box.body.GetPosition() + n * penetration);

    glm::vec3 v = box.body.GetVelocity();
    const float vN = glm::dot(v, n);
    if (vN < 0.0f)
    {
        if (m_UseBounce)
        {
            const float e = box.body.GetRestitution();
            v = v - (1.0f + e) * vN * n;
        }
        else
        {
            v -= vN * n;
        }
        box.body.SetVelocity(v);
    }
}

void CollisionScenario::ResolveBoxBox(BoxInstance& a, BoxInstance& b)
{
    auto* aCol = a.body.GetColliderAs<BoxCollider>();
    auto* bCol = b.body.GetColliderAs<BoxCollider>();
    if (!aCol || !bCol) return;

    SimCollision::OBB A{ aCol->GetCenter(), aCol->GetOrientation(), aCol->GetHalfExtents() };
    SimCollision::OBB B{ bCol->GetCenter(), bCol->GetOrientation(), bCol->GetHalfExtents() };

    SimCollision::Contact c{};
    if (!SimCollision::OBBVsOBB(A, B, c)) return;

    const glm::vec3 n = c.normal;
    const float invA = a.body.GetInverseMass();
    const float invB = b.body.GetInverseMass();
    const float invSum = invA + invB;
    if (invSum <= 0.0f) return;

    const glm::vec3 corr = n * (c.penetration / invSum);
    a.body.SetPosition(a.body.GetPosition() - corr * invA);
    b.body.SetPosition(b.body.GetPosition() + corr * invB);

    glm::vec3 va = a.body.GetVelocity();
    glm::vec3 vb = b.body.GetVelocity();
    const glm::vec3 rel = vb - va;
    const float velN = glm::dot(rel, n);
    if (velN > 0.0f) return;

    const float e = m_UseBounce ? m_BounceRestitution : 0.0f;
    const float j = -(1.0f + e) * velN / invSum;
    const glm::vec3 impulse = j * n;

    a.body.SetVelocity(va - impulse * invA);
    b.body.SetVelocity(vb + impulse * invB);
}

void CollisionScenario::SetupTestCase(TestCase testCase) {
    ClearScene();
    m_TestCase = testCase;
    m_ElapsedTime = 0.0f;
    m_Recorded = false;
    m_MovingSphereIndex = 0;
    m_Gravity = 0.0f;
    m_OverlapCount = 0;

    switch (m_TestCase) {
    case TestCase::SphereVsSphereFixed:
        m_TestDescription = "Moving sphere vs stationary sphere";
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 0.0f, { 0.3f, 0.3f, 1.0f });
        m_TargetTime = 1.0f;
        m_ExpectedPosition = { -1.0f, 0.5f, 0.0f };
        break;

    case TestCase::SphereVsPlaneAxisAligned:
        m_TestDescription = "Moving sphere vs axis-aligned plane";
        AddPlane({ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 8.0f, 8.0f }, { 0.4f, 0.4f, 0.4f });
        AddSphere({ 0.0f, 2.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        m_TargetTime = 1.5f;
        m_ExpectedPosition = { 0.0f, 0.5f, 0.0f };
        break;

    case TestCase::SphereVsPlaneTilted:
    {
        m_TestDescription = "Moving sphere vs tilted (non-axis-aligned) plane";
        const glm::vec3 normal = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
        AddPlane({ 0.0f, 0.0f, 0.0f }, normal, { 8.0f, 8.0f }, { 0.2f, 0.6f, 0.2f });

        const glm::vec3 incoming = glm::normalize(glm::vec3(0.3f, -1.0f, -0.2f));
        AddSphere(normal * 2.5f, incoming * 1.0f, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });

        m_TargetTime = 2.0f;
        m_ExpectedPosition = normal * 0.5f;
        break;
    }

    case TestCase::SphereVsPlaneTiltedSkewed:
    default:
    {
        m_TestDescription = "Moving sphere vs skewed non-axis-aligned plane";
        const glm::vec3 normal = glm::normalize(glm::vec3(1.0f, 1.0f, 0.5f));
        AddPlane({ 0.0f, 0.0f, 0.0f }, normal, { 8.0f, 8.0f }, { 0.6f, 0.4f, 0.2f });

        const glm::vec3 incoming = glm::normalize(glm::vec3(-0.4f, -1.0f, 0.2f));
        AddSphere(normal * 3.0f, incoming * 1.5f, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });

        m_TargetTime = (3.0f - 0.5f) / 1.5f;
        m_ExpectedPosition = normal * 0.5f;
        break;
    }

    case TestCase::SphereVsSphereEqualMassOneMoving:
        m_TestDescription = "Q2: Equal mass head-on (one moving)";
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 0.3f, 0.3f, 1.0f });
        break;

    case TestCase::SphereVsSphereEqualMassBothMoving:
        m_TestDescription = "Q2: Equal mass head-on (both moving)";
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 2.0f, 0.5f, 0.0f }, { -1.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 0.3f, 0.3f, 1.0f });
        break;

    case TestCase::SphereVsSphereEqualMassGlancing:
        m_TestDescription = "Q2: Equal mass glancing collision";
        AddSphere({ -2.0f, 0.9f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 0.0f, 0.3f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 0.3f, 0.3f, 1.0f });
        break;

    case TestCase::SphereVsSphereDifferentMassOneMoving:
        m_TestDescription = "Q3: Different masses head-on (one moving)";
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 3.0f, { 0.3f, 0.8f, 1.0f });
        m_TargetTime = 1.2f;
        m_MovingSphereIndex = 0;
        break;

    case TestCase::SphereVsSphereDifferentMassBothMoving:
        m_TestDescription = "Q3: Different masses head-on (both moving)";
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 2.0f, 0.5f, 0.0f }, { -1.0f, 0.0f, 0.0f }, 0.5f, 4.0f, { 0.3f, 0.8f, 1.0f });
        m_TargetTime = 1.5f;
        m_MovingSphereIndex = 0;
        break;

    case TestCase::ElasticityOne:
        m_TestDescription = "Q5: Elasticity e=1 (perfectly elastic)";
        m_UseBounce = true;
        m_BounceRestitution = 1.0f;
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 0.3f, 0.8f, 1.0f });
        m_TargetTime = 1.2f;
        m_MovingSphereIndex = 0;
        break;

    case TestCase::ElasticityZero:
        m_TestDescription = "Q5: Elasticity e=0 (perfectly inelastic normal component)";
        m_UseBounce = true;
        m_BounceRestitution = 0.0f;
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        AddSphere({ 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 0.3f, 0.8f, 1.0f });
        m_TargetTime = 1.2f;
        m_MovingSphereIndex = 0;
        break;

    case TestCase::SphereVsCuboidAxisAligned:
        m_TestDescription = "Non-spherical: Sphere vs Cuboid (axis-aligned)";
        AddBox({ 0.0f, 0.5f, 0.0f }, glm::quat(glm::vec3(0,0,0)), { 0.8f, 0.8f, 0.8f }, { 0,0,0 }, 0.0f, { 0.3f, 0.8f, 1.0f });
        AddSphere({ -2.0f, 0.5f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        m_TargetTime = 1.0f;
        m_MovingSphereIndex = 0;
        break;

    case TestCase::SphereVsCuboidRotated:
        m_TestDescription = "Non-spherical: Sphere vs Cuboid (rotated OBB)";
        AddBox({ 0.0f, 0.6f, 0.0f }, glm::quat(glm::vec3(0.0f, glm::radians(35.0f), 0.0f)), { 0.8f, 0.6f, 0.8f }, { 0,0,0 }, 0.0f, { 0.2f, 0.7f, 0.3f });
        AddSphere({ -2.2f, 0.6f, 0.0f }, { 2.3f, 0.0f, 0.1f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f });
        m_TargetTime = 1.0f;
        m_MovingSphereIndex = 0;
        break;

    case TestCase::CuboidVsCuboidAxisAligned:
        m_TestDescription = "Non-spherical: Cuboid vs Cuboid (OBB broad correctness)";
        AddBox({ -1.8f, 0.6f, 0.0f }, glm::quat(glm::vec3(0,0,0)), { 0.6f, 0.6f, 0.6f }, { 2.0f, 0.0f, 0.0f }, 1.0f, { 0.3f, 0.8f, 1.0f });
        AddBox({  0.0f, 0.6f, 0.0f }, glm::quat(glm::vec3(0,0,0)), { 0.6f, 0.6f, 0.6f }, { 0.0f, 0.0f, 0.0f }, 0.0f, { 0.9f, 0.6f, 0.2f });
        m_TargetTime = 1.0f;
        break;

    case TestCase::CuboidVsPlaneTilted:
        m_TestDescription = "Non-spherical: Cuboid vs tilted plane";
        AddPlane({ 0.0f, 0.0f, 0.0f }, glm::normalize(glm::vec3(0.0f, 1.0f, 0.7f)), { 8.0f, 8.0f }, { 0.4f, 0.4f, 0.4f });
        AddBox({ 0.0f, 2.2f, 0.0f }, glm::quat(glm::vec3(0.2f, 0.1f, 0.0f)), { 0.6f, 0.4f, 0.7f }, { 0.0f, -1.2f, 0.0f }, 1.0f, { 0.3f, 0.8f, 1.0f });
        m_TargetTime = 1.3f;
        break;
    }
}

void CollisionScenario::ComputeOverlapCount()
{
    int overlaps = 0;

    // Sphere-sphere overlaps
    for (size_t i = 0; i < m_Spheres.size(); ++i)
    {
        for (size_t j = i + 1; j < m_Spheres.size(); ++j)
        {
            auto* a = m_Spheres[i].body.GetColliderAs<SphereCollider>();
            auto* b = m_Spheres[j].body.GetColliderAs<SphereCollider>();
            if (!a || !b) continue;

            const float r = a->GetRadius() + b->GetRadius();
            const float d2 = glm::length2(a->GetCenter() - b->GetCenter());
            if (d2 < (r * r - 1e-4f)) overlaps++;
        }
    }

    // Sphere-box overlaps
    for (auto& s : m_Spheres)
    {
        auto* sCol = s.body.GetColliderAs<SphereCollider>();
        if (!sCol) continue;

        for (auto& b : m_Boxes)
        {
            auto* bCol = b.body.GetColliderAs<BoxCollider>();
            if (!bCol) continue;

            SimCollision::OBB obb{ bCol->GetCenter(), bCol->GetOrientation(), bCol->GetHalfExtents() };
            SimCollision::Contact c{};
            if (SimCollision::SphereVsOBB(sCol->GetCenter(), sCol->GetRadius(), obb, c) && c.penetration > 1e-3f)
                overlaps++;
        }
    }

    // Box-box overlaps
    for (size_t i = 0; i < m_Boxes.size(); ++i)
    {
        for (size_t j = i + 1; j < m_Boxes.size(); ++j)
        {
            auto* aCol = m_Boxes[i].body.GetColliderAs<BoxCollider>();
            auto* bCol = m_Boxes[j].body.GetColliderAs<BoxCollider>();
            if (!aCol || !bCol) continue;

            SimCollision::OBB A{ aCol->GetCenter(), aCol->GetOrientation(), aCol->GetHalfExtents() };
            SimCollision::OBB B{ bCol->GetCenter(), bCol->GetOrientation(), bCol->GetHalfExtents() };
            SimCollision::Contact c{};
            if (SimCollision::OBBVsOBB(A, B, c) && c.penetration > 1e-3f)
                overlaps++;
        }
    }

    m_OverlapCount = overlaps;
}

void CollisionScenario::OnLoad() { SetupTestCase(m_TestCase); }

void CollisionScenario::OnUpdate(float deltaTime) {
    const auto method = m_App->GetIntegrationMethod();

    for (auto& sphere : m_Spheres)
        sphere.body.Update(deltaTime, m_Gravity, method);

    for (auto& box : m_Boxes)
        box.body.Update(deltaTime, m_Gravity, method);

    // Dynamic vs planes
    for (auto& sphere : m_Spheres)
        for (const auto& plane : m_Planes)
            if (auto* p = plane.body.GetColliderAs<PlaneCollider>()) ResolveSpherePlane(sphere, *p);

    for (auto& box : m_Boxes)
        for (const auto& plane : m_Planes)
            if (auto* p = plane.body.GetColliderAs<PlaneCollider>()) ResolveBoxPlane(box, *p);

    // Sphere-sphere
    for (size_t i = 0; i < m_Spheres.size(); ++i)
        for (size_t j = i + 1; j < m_Spheres.size(); ++j)
            ResolveSphereSphere(m_Spheres[i], m_Spheres[j]);

    // Sphere-box
    for (auto& sphere : m_Spheres)
        for (auto& box : m_Boxes)
            ResolveSphereBox(sphere, box);

    // Box-box
    for (size_t i = 0; i < m_Boxes.size(); ++i)
        for (size_t j = i + 1; j < m_Boxes.size(); ++j)
            ResolveBoxBox(m_Boxes[i], m_Boxes[j]);

    m_ElapsedTime += deltaTime;
    if (!m_Recorded && m_MovingSphereIndex >= 0 && m_MovingSphereIndex < (int)m_Spheres.size() && m_ElapsedTime >= m_TargetTime) {
        m_RecordedPosition = m_Spheres[m_MovingSphereIndex].body.GetPosition();
        m_Recorded = true;
    }

    ComputeOverlapCount();
}

void CollisionScenario::OnRender(VkCommandBuffer commandBuffer) {
    auto& material = m_App->GetMaterialSettings();

    auto drawMesh = [&](const SandboxApplication::MeshBuffers& buffers, const glm::mat4& model)
    {
        PushConstants push{};
        push.model = model;
        push.checkerColorA = material.lightColor;
        push.checkerColorB = material.darkColor;
        push.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

        vkCmdPushConstants(commandBuffer, m_App->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &push);

        VkBuffer vertexBuffers[] = { buffers.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, buffers.indexCount, 1, 0, 0, 0);
    };

    for (const auto& plane : m_Planes) drawMesh(plane.buffers, plane.body.GetTransform());
    for (const auto& box : m_Boxes) drawMesh(box.buffers, box.body.GetTransform());
    for (const auto& sphere : m_Spheres) drawMesh(sphere.buffers, sphere.body.GetTransform());
}

void CollisionScenario::OnImGui() {
    ImGui::Begin("Lab 4 Collision Scenario");

    const char* options[] = {
        "Q1: Sphere vs Sphere (Fixed Target)",
        "Q2: Equal Mass Head-On (One Moving)",
        "Q2: Equal Mass Head-On (Both Moving)",
        "Q2: Equal Mass Glancing",
        "Q3: Different Mass Head-On (One Moving)",
        "Q3: Different Mass Head-On (Both Moving)",
        "Q1: Sphere vs Plane (Axis Aligned)",
        "Q1: Sphere vs Plane (Tilted)",
        "Q1: Sphere vs Plane (Skewed Tilted)",
        "Q5: Elasticity e=1",
        "Q5: Elasticity e=0",
        "Non-spherical: Sphere vs Cuboid (AABB)",
        "Non-spherical: Sphere vs Cuboid (OBB Rotated)",
        "Non-spherical: Cuboid vs Cuboid (OBB)",
        "Non-spherical: Cuboid vs Tilted Plane"
    };

    int current = std::clamp((int)m_TestCase, 0, IM_ARRAYSIZE(options) - 1);
    if (ImGui::Combo("Test Case", &current, options, IM_ARRAYSIZE(options))) {
        SetupTestCase((TestCase)current);
    }

    ImGui::Checkbox("Use Impulse Response", &m_UseBounce);
    ImGui::SliderFloat("Restitution (e)", &m_BounceRestitution, 0.0f, 1.0f);

    for (auto& sphere : m_Spheres) sphere.body.SetRestitution(m_BounceRestitution);
    for (auto& box : m_Boxes) box.body.SetRestitution(m_BounceRestitution);

    ImGui::Separator();
    ImGui::TextWrapped("%s", m_TestDescription.c_str());
    ImGui::Text("Elapsed: %.3f s / Target: %.3f s", m_ElapsedTime, m_TargetTime);

    ImGui::Text("Overlap Count (should be 0): %d", m_OverlapCount);
    if (m_OverlapCount == 0) ImGui::Text("Status: PASS (no penetration)");
    else ImGui::Text("Status: FAIL (penetration detected)");

    ImGui::Separator();
    ImGui::Text("Expected Position: (%.3f, %.3f, %.3f)", m_ExpectedPosition.x, m_ExpectedPosition.y, m_ExpectedPosition.z);

    const glm::vec3 actual = m_Recorded
        ? m_RecordedPosition
        : (m_MovingSphereIndex >= 0 && m_MovingSphereIndex < (int)m_Spheres.size()
            ? m_Spheres[m_MovingSphereIndex].body.GetPosition()
            : glm::vec3(0.0f));

    ImGui::Text("Actual Position:   (%.3f, %.3f, %.3f)", actual.x, actual.y, actual.z);

    const glm::vec3 error = actual - m_ExpectedPosition;
    ImGui::Text("Error:             (%.3f, %.3f, %.3f)", error.x, error.y, error.z);

    if (ImGui::Button("Reset Test")) SetupTestCase(m_TestCase);

    ImGui::End();
}

void CollisionScenario::OnUnload() { ClearScene(); }

void CollisionScenario::GetSelectionItems(std::vector<SceneSelectionItem>& out)
{
    out.clear();
    // spheres (IDs 1000+index)
    for (size_t i = 0; i < m_Spheres.size(); ++i) {
        SceneSelectionItem item{};
        item.id = static_cast<uint32_t>(1000 + i);
        item.name = "Sphere " + std::to_string(i);
        item.GetTransform = [this, i]() -> TransformProxy {
            TransformProxy tp{};
            const auto& body = m_Spheres[i].body;
            tp.position = body.GetPosition();
            tp.rotationDeg = glm::degrees(glm::eulerAngles(body.GetOrientation()));
            tp.scale = glm::vec3(m_Spheres[i].body.GetRadius() * 2.0f);
            return tp;
        };
        item.SetTransform = [this, i](const TransformProxy& tp) {
            auto& body = m_Spheres[i].body;
            body.SetPosition(tp.position);
            body.SetOrientation(glm::quat(glm::radians(tp.rotationDeg)));
            // radius/scale mapping omitted for spheres (leave geometry unchanged)
        };
        item.Delete = [this, i]() {
            m_App->DestroyMeshBuffers(m_Spheres[i].buffers);
            m_Spheres.erase(m_Spheres.begin() + static_cast<int>(i));
        };
        out.push_back(std::move(item));
    }

    // boxes (IDs 2000+index)
    for (size_t i = 0; i < m_Boxes.size(); ++i) {
        SceneSelectionItem item{};
        item.id = static_cast<uint32_t>(2000 + i);
        item.name = "Box " + std::to_string(i);
        item.GetTransform = [this, i]() -> TransformProxy {
            TransformProxy tp{};
            const auto& body = m_Boxes[i].body;
            tp.position = body.GetPosition();
            tp.rotationDeg = glm::degrees(glm::eulerAngles(body.GetOrientation()));
            tp.scale = m_Boxes[i].halfExtents * 2.0f;
            return tp;
        };
        item.SetTransform = [this, i](const TransformProxy& tp) {
            auto& body = m_Boxes[i].body;
            body.SetPosition(tp.position);
            body.SetOrientation(glm::quat(glm::radians(tp.rotationDeg)));
            // To change collider/mesh size you must update halfExtents + rebuild mesh; omitted here
        };
        // Leave Delete empty if you prefer not to allow deletion for boxes:
        out.push_back(std::move(item));
    }
}