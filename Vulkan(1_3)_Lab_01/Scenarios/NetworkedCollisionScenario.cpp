#include "NetworkedCollisionScenario.h"

#include "../SimulationLibrary/CollisionUtil.h"

#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#ifdef _WIN32
namespace
{
    DWORD_PTR GetNetworkingAffinityMask()
    {
        const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (count == 0) return 0;

        DWORD_PTR mask = 0;
        // Prefer cores 2–3 (spec wording), but keep safe fallback.
        if (count > 1) mask |= (static_cast<DWORD_PTR>(1) << 1);
        if (count > 2) mask |= (static_cast<DWORD_PTR>(1) << 2);
        if (mask == 0) mask = (static_cast<DWORD_PTR>(1) << 0);
        return mask;
    }
}
#endif

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

void NetworkedCollisionScenario::ClearScene()
{
    for (auto& s : m_Spheres) m_App->DestroyMeshBuffers(s.buffers);
    for (auto& p : m_Planes) m_App->DestroyMeshBuffers(p.buffers);
    for (auto& b : m_Boxes)  m_App->DestroyMeshBuffers(b.buffers);

    m_Spheres.clear();
    m_Planes.clear();
    m_Boxes.clear();
    m_NextObjectId = 1;
    m_NetTick = 0;
}

void NetworkedCollisionScenario::UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position)
{
    glm::mat4 transform = glm::mat4_cast(plane.orientation);
    transform[3] = glm::vec4(position, 1.0f);
    plane.body.SetTransform(transform);
}

void NetworkedCollisionScenario::AddPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec2& size, const glm::vec3& color)
{
    PlaneInstance instance{};
    instance.size = size;
    instance.color = color;

    const glm::vec3 n = glm::normalize(normal);
    instance.orientation = glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), n);

    instance.body.SetCollider(std::make_unique<PlaneCollider>(n));
    instance.body.SetMass(0.0f);
    UpdatePlaneTransform(instance, point);

    instance.mesh = MeshGenerator::GeneratePlane(size.x, size.y, 10, 10, color);
    instance.buffers = m_App->UploadMesh(instance.mesh);
    m_Planes.push_back(std::move(instance));
}

void NetworkedCollisionScenario::AddSphere(const glm::vec3& position, const glm::vec3& velocity, float radius, float mass, const glm::vec3& color, SimRuntime::OwnerType owner)
{
    SphereInstance s{};
    s.id = m_NextObjectId++;
    s.owner = owner;
    s.isLocallyOwned = (m_LocalPeerOwner == owner);
    s.color = color;

    s.body.SetCollider(std::make_unique<SphereCollider>(radius));
    s.body.SetPosition(position);
    s.body.SetVelocity(velocity);
    s.body.SetRadius(radius);
    s.body.SetMass(mass);
    s.body.SetRestitution(m_BounceRestitution);

    s.mesh = MeshGenerator::GenerateSphere(radius, 32, 16, color);
    s.buffers = m_App->UploadMesh(s.mesh);

    m_Spheres.push_back(std::move(s));
}

void NetworkedCollisionScenario::AddBox(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& halfExtents, const glm::vec3& velocity, float mass, const glm::vec3& color, SimRuntime::OwnerType owner)
{
    BoxInstance b{};
    b.id = m_NextObjectId++;
    b.owner = owner;
    b.isLocallyOwned = (m_LocalPeerOwner == owner);
    b.color = color;
    b.halfExtents = glm::max(halfExtents, glm::vec3(0.05f));

    b.body.SetCollider(std::make_unique<BoxCollider>(b.halfExtents));
    b.body.SetPosition(position);
    b.body.SetOrientation(orientation);
    b.body.SetVelocity(velocity);
    b.body.SetMass(mass);
    b.body.SetRestitution(m_BounceRestitution);

    b.mesh = BuildCuboidMesh(b.halfExtents, color);
    b.buffers = m_App->UploadMesh(b.mesh);

    m_Boxes.push_back(std::move(b));
}

void NetworkedCollisionScenario::AddStaticCylinderObstacle(const glm::vec3& position, float radius, float height, const glm::quat& orientation, const glm::vec3& color)
{
    BoxInstance b{};
    b.id = m_NextObjectId++;
    b.owner = SimRuntime::OwnerType::One;
    b.isLocallyOwned = false; // never replicate / simulate
    b.color = color;

    const glm::vec3 halfExtents{ radius, height * 0.5f, radius };
    b.halfExtents = glm::max(halfExtents, glm::vec3(0.05f));

    b.body.SetCollider(std::make_unique<BoxCollider>(b.halfExtents));
    b.body.SetPosition(position);
    b.body.SetOrientation(orientation);
    b.body.SetVelocity({ 0,0,0 });
    b.body.SetMass(0.0f);
    b.body.SetRestitution(m_BounceRestitution);

    // Render as cylinder, collide as box-approx
    b.mesh = MeshGenerator::GenerateCylinder(radius, height, 24, color);
    b.buffers = m_App->UploadMesh(b.mesh);

    m_Boxes.push_back(std::move(b));
}

void NetworkedCollisionScenario::AddStaticCapsuleObstacle(const glm::vec3& position, float radius, float height, const glm::quat& orientation, const glm::vec3& color)
{
    BoxInstance b{};
    b.id = m_NextObjectId++;
    b.owner = SimRuntime::OwnerType::One;
    b.isLocallyOwned = false; // never replicate / simulate
    b.color = color;

    // Capsule AABB half extents approx
    const glm::vec3 halfExtents{ radius, height * 0.5f + radius, radius };
    b.halfExtents = glm::max(halfExtents, glm::vec3(0.05f));

    b.body.SetCollider(std::make_unique<BoxCollider>(b.halfExtents));
    b.body.SetPosition(position);
    b.body.SetOrientation(orientation);
    b.body.SetVelocity({ 0,0,0 });
    b.body.SetMass(0.0f);
    b.body.SetRestitution(m_BounceRestitution);

    // Render as capsule, collide as box-approx
    b.mesh = MeshGenerator::GenerateCapsule(radius, height, 24, 12, color);
    b.buffers = m_App->UploadMesh(b.mesh);

    m_Boxes.push_back(std::move(b));
}

void NetworkedCollisionScenario::SetupPreset(DemoPreset preset)
{
    ClearScene();
    m_Preset = preset;

    // All peers build the same IDs in the same order; only the selected owner simulates dynamics.
    const auto dynOwner = m_SimOwner;

    switch (m_Preset) {
    case DemoPreset::BounceCorner_SpherePlaneWall:
    default:
        AddPlane({ 0, 0, 0 }, { 0, 1, 0 }, { 12, 12 }, { 0.4f, 0.4f, 0.4f }); // ground
        AddPlane({ 4, 0, 0 }, { -1, 0, 0 }, { 12, 6 }, { 0.45f, 0.35f, 0.35f }); // wall x=4
        AddPlane({ 0, 0, -4 }, { 0, 0, 1 }, { 12, 6 }, { 0.35f, 0.45f, 0.35f }); // wall z=-4
        m_Gravity = -9.81f;
        AddSphere({ -1.0f, 2.5f, 1.0f }, { 3.5f, 0.0f, -3.0f }, 0.45f, 1.0f, { 1.0f, 0.3f, 0.3f }, dynOwner);
        AddBox({ 1.2f, 0.7f, -1.0f }, glm::quat(glm::vec3(0.0f, glm::radians(35.0f), 0.0f)),
               { 0.6f, 0.6f, 0.6f }, { 0,0,0 }, 0.0f, { 0.2f, 0.7f, 0.3f }, dynOwner);
        break;

    case DemoPreset::SphereVsSphere_HeadOn:
        AddPlane({ 0, 0, 0 }, { 0, 1, 0 }, { 12, 12 }, { 0.4f, 0.4f, 0.4f });
        m_Gravity = 0.0f;
        AddSphere({ -3.0f, 0.5f, 0.0f }, { 3.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f }, dynOwner);
        AddSphere({  1.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, 1.0f, { 0.3f, 0.3f, 1.0f }, dynOwner);
        break;

    case DemoPreset::SphereVsCuboid_RotatedOBB:
        AddPlane({ 0, 0, 0 }, { 0, 1, 0 }, { 12, 12 }, { 0.4f, 0.4f, 0.4f });
        m_Gravity = 0.0f;
        AddBox({ 0.0f, 0.6f, 0.0f }, glm::quat(glm::vec3(0.0f, glm::radians(35.0f), 0.0f)),
               { 0.9f, 0.55f, 0.9f }, { 0,0,0 }, 0.0f, { 0.2f, 0.7f, 0.3f }, dynOwner);
        AddSphere({ -3.2f, 0.6f, 0.1f }, { 3.2f, 0.0f, 0.0f }, 0.5f, 1.0f, { 1.0f, 0.3f, 0.3f }, dynOwner);
        break;

    case DemoPreset::CuboidVsCuboid_AxisAligned:
        AddPlane({ 0, 0, 0 }, { 0, 1, 0 }, { 12, 12 }, { 0.4f, 0.4f, 0.4f });
        m_Gravity = 0.0f;
        AddBox({ -3.0f, 0.6f, 0.0f }, glm::quat(glm::vec3(0,0,0)), { 0.7f, 0.7f, 0.7f }, { 2.8f, 0.0f, 0.0f }, 1.0f, { 0.3f, 0.8f, 1.0f }, dynOwner);
        AddBox({  0.3f, 0.6f, 0.0f }, glm::quat(glm::vec3(0,0,0)), { 0.7f, 0.7f, 0.7f }, { 0.0f, 0.0f, 0.0f }, 1.0f, { 0.9f, 0.6f, 0.2f }, dynOwner);
        break;

    case DemoPreset::CuboidVsPlane_Tilted:
        AddPlane({ 0, 0, 0 }, glm::normalize(glm::vec3(0.0f, 1.0f, 0.7f)), { 12, 12 }, { 0.4f, 0.4f, 0.4f });
        m_Gravity = -9.81f;
        AddBox({ 0.0f, 3.0f, 0.0f }, glm::quat(glm::vec3(0.2f, 0.1f, 0.0f)), { 0.7f, 0.45f, 0.85f }, { 0.0f, -0.5f, 0.0f }, 1.0f, { 0.3f, 0.8f, 1.0f }, dynOwner);
        break;

    case DemoPreset::Arena_ManyObjects_Spawners:
    {
        // “Almost enclosed”: 6 planes form a box arena.
        // Gravity off -> perpetual motion.
        m_Gravity = 0.0f;

        const float halfX = 6.0f;
        const float halfY = 3.5f;
        const float halfZ = 6.0f;

        AddPlane({ 0, -halfY, 0 }, { 0,  1, 0 }, { 14, 14 }, { 0.35f, 0.35f, 0.35f }); // floor
        AddPlane({ 0,  halfY, 0 }, { 0, -1, 0 }, { 14, 14 }, { 0.30f, 0.30f, 0.35f }); // ceiling
        AddPlane({ -halfX, 0, 0 }, {  1, 0, 0 }, { 14, 7 },  { 0.35f, 0.25f, 0.25f }); // left
        AddPlane({  halfX, 0, 0 }, { -1, 0, 0 }, { 14, 7 },  { 0.35f, 0.25f, 0.25f }); // right
        AddPlane({ 0, 0, -halfZ }, { 0, 0,  1 }, { 14, 7 },  { 0.25f, 0.35f, 0.25f }); // back
        AddPlane({ 0, 0,  halfZ }, { 0, 0, -1 }, { 14, 7 },  { 0.25f, 0.35f, 0.25f }); // front

        // Static obstacles: capsule + cylinder (rendered as such; collide as box approximation)
        AddStaticCapsuleObstacle({ -1.5f, -0.5f,  0.0f }, 0.35f, 1.6f, glm::quat(glm::vec3(0.0f, glm::radians(25.0f), 0.0f)), { 0.7f, 0.7f, 0.2f });
        AddStaticCylinderObstacle({  1.8f,  0.0f, -1.5f }, 0.40f, 2.0f, glm::quat(glm::vec3(0.0f, glm::radians(-15.0f), 0.0f)), { 0.2f, 0.7f, 0.7f });
        AddStaticCapsuleObstacle({  2.5f,  0.5f,  2.0f }, 0.25f, 1.2f, glm::quat(glm::vec3(0.0f, glm::radians(60.0f), 0.0f)), { 0.8f, 0.4f, 0.2f });
        AddStaticCylinderObstacle({ -2.8f, -0.2f,  2.2f }, 0.30f, 1.8f, glm::quat(glm::vec3(0.0f, glm::radians(10.0f), 0.0f)), { 0.4f, 0.8f, 0.3f });

        // Deterministic random build so peers match.
        m_Rng.seed(1337u);
        std::uniform_real_distribution<float> px(-halfX + 1.0f, halfX - 1.0f);
        std::uniform_real_distribution<float> py(-halfY + 1.0f, halfY - 1.0f);
        std::uniform_real_distribution<float> pz(-halfZ + 1.0f, halfZ - 1.0f);

        std::uniform_real_distribution<float> massDist(0.5f, 6.0f);
        std::uniform_real_distribution<float> speedDist(2.5f, 10.0f);
        std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> radDist(0.20f, 0.45f);

        auto randDir = [&]() {
            glm::vec3 d;
            do {
                d = { dirDist(m_Rng), dirDist(m_Rng) * 0.35f, dirDist(m_Rng) };
            } while (glm::length2(d) < 1e-4f);
            return glm::normalize(d);
        };

        // 16 spheres
        for (int i = 0; i < 16; ++i) {
            const float r = radDist(m_Rng);
            const float m = massDist(m_Rng);
            const float sp = speedDist(m_Rng);
            const glm::vec3 pos{ px(m_Rng), py(m_Rng), pz(m_Rng) };
            const glm::vec3 vel = randDir() * sp;

            const glm::vec3 col = (i % 2 == 0) ? glm::vec3(1.0f, 0.35f, 0.35f) : glm::vec3(0.35f, 0.6f, 1.0f);
            AddSphere(pos, vel, r, m, col, dynOwner);
        }

        // 4 moving cuboids
        std::uniform_real_distribution<float> heDist(0.25f, 0.55f);
        std::uniform_real_distribution<float> yawDist(-glm::pi<float>(), glm::pi<float>());

        for (int i = 0; i < 4; ++i) {
            const glm::vec3 he{ heDist(m_Rng), heDist(m_Rng), heDist(m_Rng) };
            const float m = massDist(m_Rng) + 2.0f;
            const float sp = speedDist(m_Rng);
            const glm::vec3 pos{ px(m_Rng), py(m_Rng), pz(m_Rng) };
            const glm::vec3 vel = randDir() * sp;
            const glm::quat q = glm::quat(glm::vec3(0.0f, yawDist(m_Rng), 0.0f));

            AddBox(pos, q, he, vel, m, { 0.3f, 0.85f, 1.0f }, dynOwner);
        }
        break;
    }
    }

    // Refresh ownership flags after rebuild
    for (auto& s : m_Spheres) s.isLocallyOwned = (m_LocalPeerOwner == s.owner);
    for (auto& b : m_Boxes)  b.isLocallyOwned = (m_LocalPeerOwner == b.owner);

    m_TxPackets.store(0);
    m_RxPackets.store(0);
}

void NetworkedCollisionScenario::ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane)
{
    auto* sCol = sphere.body.GetColliderAs<SphereCollider>();
    if (!sCol) return;

    const float distance = plane.DistanceToPoint(sCol->GetCenter());
    const float radius = sCol->GetRadius();

    if (distance < radius) {
        const float penetration = radius - distance;
        const glm::vec3 n = plane.GetNormal();

        sphere.body.SetPosition(sphere.body.GetPosition() + n * penetration);

        glm::vec3 v = sphere.body.GetVelocity();
        const float vN = glm::dot(v, n);

        if (vN < 0.0f) {
            const float e = m_UseBounce ? sphere.body.GetRestitution() : 0.0f;
            v = v - (1.0f + e) * vN * n;
            sphere.body.SetVelocity(v);
        }
    }
}

void NetworkedCollisionScenario::ResolveSphereSphere(SphereInstance& a, SphereInstance& b)
{
    auto* aCol = a.body.GetColliderAs<SphereCollider>();
    auto* bCol = b.body.GetColliderAs<SphereCollider>();
    if (!aCol || !bCol) return;

    const glm::vec3 d = bCol->GetCenter() - aCol->GetCenter();
    const float dist = glm::length(d);
    const float rSum = aCol->GetRadius() + bCol->GetRadius();
    if (dist <= 0.0f || dist >= rSum) return;

    const glm::vec3 n = d / dist;
    const float penetration = rSum - dist;

    const float invA = a.body.GetInverseMass();
    const float invB = b.body.GetInverseMass();
    const float invSum = invA + invB;
    if (invSum <= 0.0f) return;

    const glm::vec3 corr = n * (penetration / invSum);
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

void NetworkedCollisionScenario::ResolveSphereBox(SphereInstance& sphere, BoxInstance& box)
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

    const glm::vec3 n = c.normal;
    const float invS = sphere.body.GetInverseMass();
    const float invB = box.body.GetInverseMass();
    const float invSum = invS + invB;
    if (invSum <= 0.0f) return;

    const glm::vec3 corr = n * (c.penetration / invSum);
    box.body.SetPosition(box.body.GetPosition() - corr * invB);
    sphere.body.SetPosition(sphere.body.GetPosition() + corr * invS);

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

void NetworkedCollisionScenario::ResolveBoxPlane(BoxInstance& box, const PlaneCollider& plane)
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
    if (vN < 0.0f) {
        const float e = m_UseBounce ? box.body.GetRestitution() : 0.0f;
        v = v - (1.0f + e) * vN * n;
        box.body.SetVelocity(v);
    }
}

void NetworkedCollisionScenario::ResolveBoxBox(BoxInstance& a, BoxInstance& b)
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

void NetworkedCollisionScenario::ApplyRemoteSmoothing(float dt)
{
    const float a = glm::clamp(dt * m_RemoteInterpRate, 0.0f, 1.0f);

    for (auto& s : m_Spheres) {
        if (s.isLocallyOwned) continue;
        if (!s.hasReplicatedState) continue;

        const glm::vec3 p = s.body.GetPosition();
        const glm::vec3 d = s.replicatedTargetPos - p;
        if (glm::length(d) > m_RemoteSnapDistance) {
            s.body.SetPosition(s.replicatedTargetPos);
            s.body.SetVelocity(s.replicatedTargetVel);
        }
        else {
            s.body.SetPosition(glm::mix(p, s.replicatedTargetPos, a));
            s.body.SetVelocity(glm::mix(s.body.GetVelocity(), s.replicatedTargetVel, a));
        }
    }

    for (auto& b : m_Boxes) {
        if (b.isLocallyOwned) continue;
        if (!b.hasReplicatedState) continue;

        const glm::vec3 p = b.body.GetPosition();
        const glm::vec3 d = b.replicatedTargetPos - p;
        if (glm::length(d) > m_RemoteSnapDistance) {
            b.body.SetPosition(b.replicatedTargetPos);
            b.body.SetVelocity(b.replicatedTargetVel);
        }
        else {
            b.body.SetPosition(glm::mix(p, b.replicatedTargetPos, a));
            b.body.SetVelocity(glm::mix(b.body.GetVelocity(), b.replicatedTargetVel, a));
        }
    }
}

void NetworkedCollisionScenario::SendOwnedStates_NoLock()
{
    if (!m_NetworkingActive) return;

    for (const auto& s : m_Spheres) {
        if (!s.isLocallyOwned) continue;

        SimStatePacket p{};
        p.objectId = s.id;
        p.owner = static_cast<uint8_t>(s.owner);
        const glm::vec3 pos = s.body.GetPosition();
        const glm::vec3 vel = s.body.GetVelocity();
        p.pos[0] = pos.x; p.pos[1] = pos.y; p.pos[2] = pos.z;
        p.vel[0] = vel.x; p.vel[1] = vel.y; p.vel[2] = vel.z;
        p.tick = m_NetTick;
        m_Network.SendState(p);
        m_TxPackets.fetch_add(1);
    }

    for (const auto& b : m_Boxes) {
        if (!b.isLocallyOwned) continue;

        SimStatePacket p{};
        p.objectId = b.id;
        p.owner = static_cast<uint8_t>(b.owner);
        const glm::vec3 pos = b.body.GetPosition();
        const glm::vec3 vel = b.body.GetVelocity();
        p.pos[0] = pos.x; p.pos[1] = pos.y; p.pos[2] = pos.z;
        p.vel[0] = vel.x; p.vel[1] = vel.y; p.vel[2] = vel.z;
        p.tick = m_NetTick;
        m_Network.SendState(p);
        m_TxPackets.fetch_add(1);
    }

    ++m_NetTick;
}

void NetworkedCollisionScenario::ReceiveRemoteCommands_NoLock()
{
    if (!m_NetworkingActive) return;

    const auto cmds = m_Network.ReceiveCommands();
    for (const auto& c : cmds) {
        if (c.command == NetCommandType::SetPreset) {
            m_PendingPreset.store(static_cast<int>(c.value + 0.5f));
        }
        else if (c.command == NetCommandType::Reset) {
            m_PendingReset.store(true);
        }
    }
}

void NetworkedCollisionScenario::ReceiveRemoteStates_NoLock(float dt)
{
    if (!m_NetworkingActive) return;

    // Helper lambda to actually apply the packet to the objects
    auto applyPacket = [this](const SimStatePacket& p) {
        const uint32_t id = p.objectId;
        const glm::vec3 pos{ p.pos[0], p.pos[1], p.pos[2] };
        const glm::vec3 vel{ p.vel[0], p.vel[1], p.vel[2] };
        bool applied = false;

        for (auto& s : m_Spheres) {
            if (s.id != id) continue;
            if (s.isLocallyOwned) { applied = true; break; }
            s.replicatedTargetPos = pos;
            s.replicatedTargetVel = vel;
            s.hasReplicatedState = true;
            applied = true;
            break;
        }
        if (!applied) {
            for (auto& b : m_Boxes) {
                if (b.id != id) continue;
                if (b.isLocallyOwned) { applied = true; break; }
                b.replicatedTargetPos = pos;
                b.replicatedTargetVel = vel;
                b.hasReplicatedState = true;
                break;
            }
        }
        m_RxPackets.fetch_add(1);
        };

    const bool emulate = m_EnableNetEmulation.load();
    const float baseMs = m_EmuBaseLatencyMs.load();
    const float jitterMs = m_EmuJitterMs.load();
    const float lossPct = m_EmuLossPercent.load();
    std::uniform_real_distribution<float> lossDist(0.0f, 100.0f);
    std::uniform_real_distribution<float> jitterDist(-jitterMs, jitterMs);

    // Read all incoming packets
    const auto packets = m_Network.ReceiveStates();
    for (const auto& p : packets) {
        if (emulate) {
            if (lossDist(m_NetRng) < lossPct) continue; // Simulate Packet Loss (Drop it)

            // Simulate Latency & Jitter
            const float delayedMs = std::max(0.0f, baseMs + jitterDist(m_NetRng));
            m_DelayedIncomingStates.push_back({ p, delayedMs * 0.001f });
        }
        else {
            applyPacket(p);
        }
    }

    // Process delayed packets
    if (emulate) {
        for (auto it = m_DelayedIncomingStates.begin(); it != m_DelayedIncomingStates.end(); ) {
            it->remainingDelaySec -= dt;
            if (it->remainingDelaySec <= 0.0f) {
                applyPacket(it->packet);
                it = m_DelayedIncomingStates.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

void NetworkedCollisionScenario::SendSpawn_NoLock(const SimSpawnPacket& p)
{
    if (!m_NetworkingActive) return;
    m_Network.SendSpawn(p);
}

void NetworkedCollisionScenario::ReceiveRemoteSpawns_NoLock()
{
    if (!m_NetworkingActive) return;

    const auto spawns = m_Network.ReceiveSpawns();
    for (const auto& p : spawns) {
        const uint32_t id = p.objectId;

        // already exists?
        bool exists = false;
        for (const auto& s : m_Spheres) if (s.id == id) { exists = true; break; }
        if (!exists) for (const auto& b : m_Boxes) if (b.id == id) { exists = true; break; }
        if (exists) continue;

        const auto owner = static_cast<SimRuntime::OwnerType>(p.owner);
        const auto shape = static_cast<SimRuntime::SpawnerShapeType>(p.shape);

        if (shape == SimRuntime::SpawnerShapeType::Sphere) {
            SphereInstance s{};
            s.id = id;
            s.owner = owner;
            s.isLocallyOwned = (m_LocalPeerOwner == owner);
            s.color = { 1.0f, 0.55f, 0.25f };

            const float r = std::max(0.05f, p.radius);
            const float mass = std::max(0.0f, p.mass);

            s.body.SetCollider(std::make_unique<SphereCollider>(r));
            s.body.SetPosition({ p.pos[0], p.pos[1], p.pos[2] });
            s.body.SetVelocity({ p.vel[0], p.vel[1], p.vel[2] });
            s.body.SetRadius(r);
            s.body.SetMass(mass);
            s.body.SetRestitution(m_BounceRestitution);

            s.mesh = MeshGenerator::GenerateSphere(r, 32, 16, s.color);
            s.buffers = m_App->UploadMesh(s.mesh);
            m_Spheres.push_back(std::move(s));
        }
        else {
            BoxInstance b{};
            b.id = id;
            b.owner = owner;
            b.isLocallyOwned = (m_LocalPeerOwner == owner);
            b.color = { 0.25f, 0.85f, 0.55f };

            const glm::vec3 he = glm::max(glm::vec3(0.05f), glm::vec3(p.size[0], p.size[1], p.size[2]) * 0.5f);
            const float mass = std::max(0.0f, p.mass);

            b.halfExtents = he;
            b.body.SetCollider(std::make_unique<BoxCollider>(b.halfExtents));
            b.body.SetPosition({ p.pos[0], p.pos[1], p.pos[2] });
            b.body.SetOrientation(glm::quat(glm::vec3(0, 0, 0)));
            b.body.SetVelocity({ p.vel[0], p.vel[1], p.vel[2] });
            b.body.SetMass(mass);
            b.body.SetRestitution(m_BounceRestitution);

            b.mesh = BuildCuboidMesh(b.halfExtents, b.color);
            b.buffers = m_App->UploadMesh(b.mesh);
            m_Boxes.push_back(std::move(b));
        }
    }
}

void NetworkedCollisionScenario::EnforceMinimumSpeed_NoLock()
{
    if (m_MinDynamicSpeed <= 0.0f) return;

    auto enforce = [&](PhysicsObject& body)
    {
        if (body.GetInverseMass() <= 0.0f) return;

        glm::vec3 v = body.GetVelocity();
        const float s2 = glm::length2(v);
        if (s2 >= m_MinDynamicSpeed * m_MinDynamicSpeed) return;

        // If velocity is near zero, choose a stable direction.
        if (s2 < 1e-8f) {
            v = { 1.0f, 0.0f, 0.0f };
        }
        v = glm::normalize(v) * m_MinDynamicSpeed;
        body.SetVelocity(v);
    };

    for (auto& s : m_Spheres) if (s.isLocallyOwned) enforce(s.body);
    for (auto& b : m_Boxes)   if (b.isLocallyOwned) enforce(b.body);
}

void NetworkedCollisionScenario::SpawnSphereFromUI_NoLock()
{
    if (m_Preset != DemoPreset::Arena_ManyObjects_Spawners) return;

    const float r = std::max(0.05f, m_SpawnSphereRadius);
    const float mass = std::max(0.0f, m_SpawnMass);
    const float speed = std::max(0.0f, m_SpawnSpeed);

    // spawn near one wall, shoot roughly toward center with spread
    std::uniform_real_distribution<float> a(-glm::radians(m_SpawnSpreadDeg), glm::radians(m_SpawnSpreadDeg));
    const float yaw = a(m_Rng);

    glm::vec3 dir = glm::normalize(glm::vec3(1.0f, 0.05f, 0.0f));
    dir = glm::mat3_cast(glm::angleAxis(yaw, glm::vec3(0, 1, 0))) * dir;

    const glm::vec3 pos{ -5.2f, 0.0f, 0.0f };
    const glm::vec3 vel = dir * speed;

    // Spawn as sim-owner so collisions are authoritative and consistent
    AddSphere(pos, vel, r, mass, { 1.0f, 0.65f, 0.25f }, m_SimOwner);

    // replicate spawn to other peer(s)
    SimSpawnPacket p{};
    p.objectId = m_Spheres.back().id;
    p.owner = static_cast<uint8_t>(m_SimOwner);
    p.shape = static_cast<uint8_t>(SimRuntime::SpawnerShapeType::Sphere);
    strncpy_s(p.material, "ui_spawn", _TRUNCATE);
    p.pos[0] = pos.x; p.pos[1] = pos.y; p.pos[2] = pos.z;
    p.vel[0] = vel.x; p.vel[1] = vel.y; p.vel[2] = vel.z;
    p.radius = r;
    p.height = 0.0f;
    p.mass = mass;
    p.tick = m_NetTick;

    SendSpawn_NoLock(p);
}

void NetworkedCollisionScenario::SpawnBoxFromUI_NoLock()
{
    if (m_Preset != DemoPreset::Arena_ManyObjects_Spawners) return;

    const glm::vec3 he = glm::max(glm::vec3(0.05f), m_SpawnBoxHalfExtents);
    const float mass = std::max(0.0f, m_SpawnMass);
    const float speed = std::max(0.0f, m_SpawnSpeed);

    std::uniform_real_distribution<float> a(-glm::radians(m_SpawnSpreadDeg), glm::radians(m_SpawnSpreadDeg));
    const float yaw = a(m_Rng);

    glm::vec3 dir = glm::normalize(glm::vec3(1.0f, 0.05f, 0.0f));
    dir = glm::mat3_cast(glm::angleAxis(yaw, glm::vec3(0, 1, 0))) * dir;

    const glm::vec3 pos{ -5.2f, 0.0f, -1.0f };
    const glm::vec3 vel = dir * speed;

    AddBox(pos, glm::quat(glm::vec3(0, 0, 0)), he, vel, mass, { 0.25f, 0.85f, 0.55f }, m_SimOwner);

    SimSpawnPacket p{};
    p.objectId = m_Boxes.back().id;
    p.owner = static_cast<uint8_t>(m_SimOwner);
    p.shape = static_cast<uint8_t>(SimRuntime::SpawnerShapeType::Cuboid);
    strncpy_s(p.material, "ui_spawn", _TRUNCATE);
    p.pos[0] = pos.x; p.pos[1] = pos.y; p.pos[2] = pos.z;
    p.vel[0] = vel.x; p.vel[1] = vel.y; p.vel[2] = vel.z;
    p.size[0] = he.x * 2.0f; p.size[1] = he.y * 2.0f; p.size[2] = he.z * 2.0f;
    p.radius = 0.0f;
    p.height = 0.0f;
    p.mass = mass;
    p.tick = m_NetTick;

    SendSpawn_NoLock(p);
}

void NetworkedCollisionScenario::StartNetworkWorker()
{
    if (m_RunNetworkThread.exchange(true)) return;

    m_NetworkThread = std::thread([this]() { NetworkWorkerMain(); });

#ifdef _WIN32
    if (const DWORD_PTR netMask = GetNetworkingAffinityMask(); netMask != 0) {
        SetThreadAffinityMask(reinterpret_cast<HANDLE>(m_NetworkThread.native_handle()), netMask);
        SetThreadDescription(reinterpret_cast<HANDLE>(m_NetworkThread.native_handle()), L"NetworkWorker_CollisionDemo");
    }
#endif
}

void NetworkedCollisionScenario::StopNetworkWorker()
{
    if (!m_RunNetworkThread.exchange(false)) return;
    if (m_NetworkThread.joinable()) m_NetworkThread.join();
}

void NetworkedCollisionScenario::NetworkWorkerMain()
{
    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (m_RunNetworkThread.load()) {
        const float hz = std::max(1.0f, m_NetworkTargetHz);
        const auto step = std::chrono::duration<float>(1.0f / hz);

        auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

#ifdef _WIN32
        m_LastNetworkCpu.store(static_cast<int>(GetCurrentProcessorNumber()));
#endif

        {
            std::lock_guard<std::mutex> lock(m_Mutex);

            ReceiveRemoteCommands_NoLock();
            ReceiveRemoteSpawns_NoLock();   // NEW
            ReceiveRemoteStates_NoLock(dt);
            SendOwnedStates_NoLock();
        }

        if (dt > 0.0001f) {
            m_NetworkMeasuredHz.store(1.0f / dt);
        }

        std::this_thread::sleep_for(step);
    }
}

void NetworkedCollisionScenario::SendSetPreset(DemoPreset preset)
{
    if (!m_NetworkingActive) return;
    SimCommandPacket c{};
    c.command = NetCommandType::SetPreset;
    c.value = static_cast<float>(static_cast<int>(preset));
    c.tick = m_NetTick;
    m_Network.SendCommand(c);
}

void NetworkedCollisionScenario::SendReset()
{
    if (!m_NetworkingActive) return;
    SimCommandPacket c{};
    c.command = NetCommandType::Reset;
    c.value = 0.0f;
    c.tick = m_NetTick;
    m_Network.SendCommand(c);
}

void NetworkedCollisionScenario::OnLoad()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    SetupPreset(m_Preset);
    StartNetworkWorker();
}

void NetworkedCollisionScenario::OnUnload()
{
    StopNetworkWorker();

    if (m_NetworkingActive) {
        m_Network.Shutdown();
        m_NetworkingActive = false;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    ClearScene();
}

void NetworkedCollisionScenario::GetSelectionItems(std::vector<SceneSelectionItem>& out)
{
    out.clear();

    std::lock_guard<std::mutex> lock(m_Mutex);

    // Expose all spheres
    for (size_t idx = 0; idx < m_Spheres.size(); ++idx) {
        auto& sphere = m_Spheres[idx];
        SceneSelectionItem item;
        item.id = sphere.id;
        item.name = "Sphere #" + std::to_string(sphere.id);

        item.GetTransform = [this, idx]() {
            if (idx >= m_Spheres.size()) return TransformProxy{};
            TransformProxy proxy;
            proxy.position = m_Spheres[idx].body.GetPosition();
            proxy.scale = glm::vec3(m_Spheres[idx].body.GetRadius() * 2.0f);
            return proxy;
            };

        item.SetTransform = [this, idx](const TransformProxy& proxy) {
            if (idx < m_Spheres.size()) {
                m_Spheres[idx].body.SetPosition(proxy.position);
                m_Spheres[idx].body.SetVelocity(glm::vec3(0.0f));
            }
            };

        out.push_back(item);
    }

    // Expose all boxes
    for (size_t idx = 0; idx < m_Boxes.size(); ++idx) {
        auto& box = m_Boxes[idx];
        SceneSelectionItem item;
        item.id = 1000000 + box.id;
        item.name = "Box #" + std::to_string(box.id);

        item.GetTransform = [this, idx]() {
            if (idx >= m_Boxes.size()) return TransformProxy{};
            TransformProxy proxy;
            proxy.position = m_Boxes[idx].body.GetPosition();
            proxy.scale = m_Boxes[idx].halfExtents * 2.0f;
            auto euler = glm::degrees(glm::eulerAngles(m_Boxes[idx].body.GetOrientation()));
            proxy.rotationDeg = euler;
            return proxy;
            };

        item.SetTransform = [this, idx](const TransformProxy& proxy) {
            if (idx < m_Boxes.size()) {
                m_Boxes[idx].body.SetPosition(proxy.position);
                m_Boxes[idx].body.SetVelocity(glm::vec3(0.0f));
                m_Boxes[idx].body.SetOrientation(glm::quat(glm::radians(proxy.rotationDeg)));
            }
            };

        out.push_back(item);
    }
}

void NetworkedCollisionScenario::OnUpdate(float deltaTime)
{
    std::lock_guard<std::mutex> lock(m_Mutex);

    const auto method = m_App->GetIntegrationMethod();

    // Integrate only locally-owned dynamic bodies
    for (auto& s : m_Spheres) {
        if (!s.isLocallyOwned) continue;
        s.body.Update(deltaTime, m_Gravity, method);
    }
    for (auto& b : m_Boxes) {
        if (!b.isLocallyOwned) continue;
        b.body.Update(deltaTime, m_Gravity, method);
    }

    // Collisions only among locally-owned dynamic bodies (keeps authority single-source)
    for (auto& s : m_Spheres) {
        if (!s.isLocallyOwned) continue;

        for (const auto& p : m_Planes) {
            if (auto* pc = p.body.GetColliderAs<PlaneCollider>()) ResolveSpherePlane(s, *pc);
        }

        for (auto& box : m_Boxes) {
            ResolveSphereBox(s, box);
        }
    }

    // sphere-sphere if both locally owned
    for (size_t i = 0; i < m_Spheres.size(); ++i) {
        for (size_t j = i + 1; j < m_Spheres.size(); ++j) {
            if (!m_Spheres[i].isLocallyOwned) continue;
            if (!m_Spheres[j].isLocallyOwned) continue;
            ResolveSphereSphere(m_Spheres[i], m_Spheres[j]);
        }
    }

    // box-plane if locally owned
    for (auto& b : m_Boxes) {
        if (!b.isLocallyOwned) continue;
        for (const auto& p : m_Planes) {
            if (auto* pc = p.body.GetColliderAs<PlaneCollider>()) ResolveBoxPlane(b, *pc);
        }
    }

    // box-box if both locally owned
    for (size_t i = 0; i < m_Boxes.size(); ++i) {
        for (size_t j = i + 1; j < m_Boxes.size(); ++j) {
            if (!m_Boxes[i].isLocallyOwned) continue;
            if (!m_Boxes[j].isLocallyOwned) continue;
            ResolveBoxBox(m_Boxes[i], m_Boxes[j]);
        }
    }

    // Keep bodies moving
    EnforceMinimumSpeed_NoLock();

    // Smooth remote replicas
    ApplyRemoteSmoothing(deltaTime);
}

void NetworkedCollisionScenario::OnRender(VkCommandBuffer commandBuffer)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto& material = m_App->GetMaterialSettings();

    auto tintForOwner = [&](SimRuntime::OwnerType owner) -> glm::vec4
        {
            if (!m_ShowOwnerTint) return { 1,1,1,1 };
            switch (owner) {
            case SimRuntime::OwnerType::One: return { 1.0f, 0.3f, 0.3f, 1.0f };
            case SimRuntime::OwnerType::Two: return { 0.3f, 1.0f, 0.3f, 1.0f };
            case SimRuntime::OwnerType::Three: return { 0.3f, 0.5f, 1.0f, 1.0f };
            case SimRuntime::OwnerType::Four: return { 1.0f, 1.0f, 0.3f, 1.0f };
            default: return { 1,1,1,1 };
            }
        };

    auto drawMesh = [&](const SandboxApplication::MeshBuffers& buffers, const glm::mat4& model, const glm::vec4& tint)
        {
            PushConstants push{};
            push.model = model;

            push.checkerColorA = glm::mix(material.lightColor, tint, 0.60f);
            push.checkerColorB = glm::mix(material.darkColor, tint, 0.35f);
            push.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

            vkCmdPushConstants(commandBuffer, m_App->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(PushConstants), &push);

            VkBuffer vertexBuffers[] = { buffers.vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, buffers.indexCount, 1, 0, 0, 0);
        };

    for (const auto& p : m_Planes) {
        drawMesh(p.buffers, p.body.GetTransform(), { 1,1,1,1 });
    }

    for (const auto& b : m_Boxes) {
        drawMesh(b.buffers, b.body.GetTransform(), tintForOwner(b.owner));
    }

    for (const auto& s : m_Spheres) {
        drawMesh(s.buffers, s.body.GetTransform(), tintForOwner(s.owner));
    }
}

void NetworkedCollisionScenario::OnImGui()
{
    ImGui::Begin("Collision (Networked)");

    // Apply remote commands on the main thread
    if (m_PendingReset.exchange(false)) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        SetupPreset(m_Preset);
    }
    if (int p = m_PendingPreset.exchange(-1); p >= 0) {
        p = std::clamp(p, 0, 5);
        std::lock_guard<std::mutex> lock(m_Mutex);
        SetupPreset(static_cast<DemoPreset>(p));
    }

    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        const char* peerNames[] = { "ONE", "TWO", "THREE", "FOUR" };
        int localPeer = static_cast<int>(m_LocalPeerOwner);
        if (ImGui::Combo("Local Peer", &localPeer, peerNames, IM_ARRAYSIZE(peerNames))) {
            m_LocalPeerOwner = static_cast<SimRuntime::OwnerType>(localPeer);
            for (auto& s : m_Spheres) s.isLocallyOwned = (m_LocalPeerOwner == s.owner);
            for (auto& b : m_Boxes)  b.isLocallyOwned = (m_LocalPeerOwner == b.owner);
        }

        int simOwner = static_cast<int>(m_SimOwner);
        if (ImGui::Combo("Sim Owner (dynamic bodies)", &simOwner, peerNames, IM_ARRAYSIZE(peerNames))) {
            m_SimOwner = static_cast<SimRuntime::OwnerType>(simOwner);
            SetupPreset(m_Preset);
            SendSetPreset(m_Preset);
        }

        const char* presets[] = {
            "Bounce: Sphere vs Plane+Walls (plus OBB obstacle)",
            "Sphere vs Sphere (head-on)",
            "Sphere vs Cuboid (rotated OBB)",
            "Cuboid vs Cuboid (axis-aligned)",
            "Cuboid vs Tilted Plane",
            "Arena: Many Spheres + Moving Cuboids + Static Capsule/Cylinder + Spawners"
        };

        int presetIndex = static_cast<int>(m_Preset);
        if (ImGui::Combo("Demo Preset", &presetIndex, presets, IM_ARRAYSIZE(presets))) {
            presetIndex = std::clamp(presetIndex, 0, IM_ARRAYSIZE(presets) - 1);
            SetupPreset(static_cast<DemoPreset>(presetIndex));
            SendSetPreset(static_cast<DemoPreset>(presetIndex));
        }

        ImGui::Checkbox("Owner Tint", &m_ShowOwnerTint);
        ImGui::Checkbox("Use Impulse (bounce)", &m_UseBounce);
        ImGui::SliderFloat("Restitution (e)", &m_BounceRestitution, 0.0f, 1.0f);
        ImGui::SliderFloat("Gravity", &m_Gravity, -30.0f, 30.0f);
        ImGui::SliderFloat("Min Dynamic Speed", &m_MinDynamicSpeed, 0.0f, 10.0f, "%.2f");

        for (auto& s : m_Spheres) s.body.SetRestitution(m_BounceRestitution);
        for (auto& b : m_Boxes)  b.body.SetRestitution(m_BounceRestitution);

        ImGui::Separator();
        if (ImGui::Button("Reset (Replicated)")) {
            SetupPreset(m_Preset);
            SendReset();
        }

        // --- NEW: UI Spawners ---
        if (m_Preset == DemoPreset::Arena_ManyObjects_Spawners) {
            ImGui::Separator();
            ImGui::Text("Spawners (Arena)");

            // keep authority clean: only sim-owner spawns
            const bool canSpawn = (m_LocalPeerOwner == m_SimOwner);
            if (!canSpawn) ImGui::Text("Spawn disabled: set Local Peer == Sim Owner to spawn.");

            if (!canSpawn) ImGui::BeginDisabled();

            ImGui::SliderFloat("Spawn Mass", &m_SpawnMass, 0.1f, 20.0f, "%.2f");
            ImGui::SliderFloat("Spawn Speed", &m_SpawnSpeed, 0.0f, 25.0f, "%.2f");
            ImGui::SliderFloat("Spawn Spread (deg)", &m_SpawnSpreadDeg, 0.0f, 45.0f, "%.1f");

            ImGui::SliderFloat("Sphere Radius", &m_SpawnSphereRadius, 0.1f, 1.0f, "%.2f");
            ImGui::SliderFloat3("Cube Half Extents", &m_SpawnBoxHalfExtents.x, 0.1f, 1.0f, "%.2f");

            if (ImGui::Button("Spawn Sphere")) {
                SpawnSphereFromUI_NoLock();
            }
            ImGui::SameLine();
            if (ImGui::Button("Spawn Cube")) {
                SpawnBoxFromUI_NoLock();
            }

            if (!canSpawn) ImGui::EndDisabled();
        }

        ImGui::Separator();
        ImGui::Text("Remote smoothing");
        ImGui::SliderFloat("Interp Rate", &m_RemoteInterpRate, 1.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Snap Distance", &m_RemoteSnapDistance, 0.1f, 10.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Networking");
        ImGui::InputInt("Local Port", &m_LocalPort);
        ImGui::InputText("Remote IP", m_RemoteIp, IM_ARRAYSIZE(m_RemoteIp));
        ImGui::InputInt("Remote Port", &m_RemotePort);

        if (!m_NetworkingActive) {
            if (ImGui::Button("Start Network")) {
                if (m_Network.Initialize(static_cast<uint16_t>(m_LocalPort))) {
                    m_NetworkingActive = m_Network.SetRemote(m_RemoteIp, static_cast<uint16_t>(m_RemotePort));
                }
            }
        }
        else {
            if (ImGui::Button("Stop Network")) {
                m_Network.Shutdown();
                m_NetworkingActive = false;
            }
        }

        ImGui::Separator();
        ImGui::Text("Robustness - Remote Smoothing");
        ImGui::SliderFloat("Remote Interp Rate", &m_RemoteInterpRate, 1.0f, 40.0f, "%.1f");
        ImGui::SliderFloat("Remote Snap Distance", &m_RemoteSnapDistance, 0.05f, 5.0f, "%.2f");
        bool emulate = m_EnableNetEmulation.load();
        if (ImGui::Checkbox("Enable Delay/Loss Emulation", &emulate)) {
            m_EnableNetEmulation.store(emulate);
        }

        float baseMs = m_EmuBaseLatencyMs.load();
        if (ImGui::SliderFloat("Emu Base Latency (ms)", &baseMs, 0.0f, 300.0f, "%.0f")) {
            m_EmuBaseLatencyMs.store(baseMs);
        }

        float jitterMs = m_EmuJitterMs.load();
        if (ImGui::SliderFloat("Emu Jitter (ms)", &jitterMs, 0.0f, 200.0f, "%.0f")) {
            m_EmuJitterMs.store(jitterMs);
        }

        float lossPct = m_EmuLossPercent.load();
        if (ImGui::SliderFloat("Emu Loss (%)", &lossPct, 0.0f, 50.0f, "%.0f")) {
            m_EmuLossPercent.store(lossPct);
        }

        ImGui::Text("Network Status: %s", m_NetworkingActive ? "ACTIVE" : "INACTIVE");
        ImGui::SliderFloat("Network Tick Hz", &m_NetworkTargetHz, 1.0f, 120.0f, "%.1f");
        ImGui::Text("Measured Network Hz: %.1f", m_NetworkMeasuredHz.load());
        ImGui::Text("Tx Packets: %llu", static_cast<unsigned long long>(m_TxPackets.load()));
        ImGui::Text("Rx Packets: %llu", static_cast<unsigned long long>(m_RxPackets.load()));
        ImGui::Text("Network CPU Index: %d", m_LastNetworkCpu.load());
    }

    ImGui::End();
}