#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "FlatBufferPreviewScenario.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>

#ifdef _WIN32
namespace
{
    DWORD_PTR GetNetworkingAffinityMask()
    {
        // Core 2-3 => CPU bits 1 and 2
        const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (count == 0) return 0;

        DWORD_PTR mask = 0;
        if (count > 1) mask |= (static_cast<DWORD_PTR>(1) << 1);
        if (count > 2) mask |= (static_cast<DWORD_PTR>(1) << 2);

        // fallback
        if (mask == 0) mask = (static_cast<DWORD_PTR>(1) << 0);
        return mask;
    }
}
#endif

namespace
{
    glm::vec3 ColorForIndex(size_t i)
    {
        static const glm::vec3 colors[] = {
            {1.0f, 0.3f, 0.3f}, {0.3f, 1.0f, 0.3f}, {0.3f, 0.5f, 1.0f},
            {1.0f, 1.0f, 0.3f}, {1.0f, 0.4f, 1.0f}, {0.3f, 1.0f, 1.0f}
        };
        return colors[i % (sizeof(colors) / sizeof(colors[0]))];
    }
}

void FlatBufferPreviewScenario::Clear() {
    for (auto& item : m_Items) {
        m_App->DestroyMeshBuffers(item.buffers);
    }
    m_Items.clear();
    m_LocalWarnings.clear();
    m_UnsupportedCount = 0;
}

glm::mat4 FlatBufferPreviewScenario::BuildModelMatrix(const SimRuntime::Transform& t) {
    glm::mat4 m(1.0f);
    m = glm::translate(m, t.position);
    m = glm::rotate(m, glm::radians(t.orientation.yaw), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(t.orientation.pitch), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(t.orientation.roll), glm::vec3(0, 0, 1));
    m = glm::scale(m, t.scale);
    return m;
}

Mesh FlatBufferPreviewScenario::BuildCuboidMesh(const glm::vec3& size, const glm::vec3& color) {
    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float hz = size.z * 0.5f;

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
        uint32_t b = f * 4;
        mesh.indices.push_back(b + 0); mesh.indices.push_back(b + 1); mesh.indices.push_back(b + 2);
        mesh.indices.push_back(b + 0); mesh.indices.push_back(b + 2); mesh.indices.push_back(b + 3);
    }

    return mesh;
}

glm::vec3 FlatBufferPreviewScenario::RandomVec3InRange(const SimRuntime::Vec3RangeDef& r)
{
    std::uniform_real_distribution<float> dx(std::min(r.min.x, r.max.x), std::max(r.min.x, r.max.x));
    std::uniform_real_distribution<float> dy(std::min(r.min.y, r.max.y), std::max(r.min.y, r.max.y));
    std::uniform_real_distribution<float> dz(std::min(r.min.z, r.max.z), std::max(r.min.z, r.max.z));
    return { dx(m_SpawnRng), dy(m_SpawnRng), dz(m_SpawnRng) };
}

SimRuntime::Transform FlatBufferPreviewScenario::BuildSpawnTransform(const SimRuntime::SpawnLocationDef& loc)
{
    SimRuntime::Transform t{};
    switch (loc.type) {
    case SimRuntime::SpawnLocationType::Fixed:
        return loc.fixedTransform;

    case SimRuntime::SpawnLocationType::RandomBox:
    {
        std::uniform_real_distribution<float> dx(std::min(loc.randomBoxMin.x, loc.randomBoxMax.x), std::max(loc.randomBoxMin.x, loc.randomBoxMax.x));
        std::uniform_real_distribution<float> dy(std::min(loc.randomBoxMin.y, loc.randomBoxMax.y), std::max(loc.randomBoxMin.y, loc.randomBoxMax.y));
        std::uniform_real_distribution<float> dz(std::min(loc.randomBoxMin.z, loc.randomBoxMax.z), std::max(loc.randomBoxMin.z, loc.randomBoxMax.z));
        t.position = { dx(m_SpawnRng), dy(m_SpawnRng), dz(m_SpawnRng) };
        t.scale = { 1.0f, 1.0f, 1.0f };
        return t;
    }

    case SimRuntime::SpawnLocationType::RandomSphere:
    {
        std::uniform_real_distribution<float> du(-1.0f, 1.0f);
        std::uniform_real_distribution<float> dr(0.0f, 1.0f);

        glm::vec3 d;
        do {
            d = { du(m_SpawnRng), du(m_SpawnRng), du(m_SpawnRng) };
        } while (glm::length2(d) < 0.0001f);

        d = glm::normalize(d);
        const float r = std::cbrt(dr(m_SpawnRng)) * std::max(0.0f, loc.randomSphereRadius);

        t.position = loc.randomSphereCenter + d * r;
        t.scale = { 1.0f, 1.0f, 1.0f };
        return t;
    }

    default:
        t.scale = { 1.0f, 1.0f, 1.0f };
        return t;
    }
}

SimRuntime::OwnerType FlatBufferPreviewScenario::ResolveSpawnerOwner(SpawnerRuntime& s)
{
    using SO = SimRuntime::SpawnerOwnerType;
    switch (s.def.base.owner) {
    case SO::One: return SimRuntime::OwnerType::One;
    case SO::Two: return SimRuntime::OwnerType::Two;
    case SO::Three: return SimRuntime::OwnerType::Three;
    case SO::Four: return SimRuntime::OwnerType::Four;
    case SO::Sequential:
    default:
    {
        const SimRuntime::OwnerType owners[] = {
            SimRuntime::OwnerType::One,
            SimRuntime::OwnerType::Two,
            SimRuntime::OwnerType::Three,
            SimRuntime::OwnerType::Four
        };
        SimRuntime::OwnerType o = owners[s.sequentialCursor % 4];
        s.sequentialCursor = (s.sequentialCursor + 1) % 4;
        return o;
    }
    }
}

bool FlatBufferPreviewScenario::IsSpawnerAuthority(const SpawnerRuntime& s) const
{
    using SO = SimRuntime::SpawnerOwnerType;
    switch (s.def.base.owner) {
    case SO::One:   return m_LocalPeerOwner == SimRuntime::OwnerType::One;
    case SO::Two:   return m_LocalPeerOwner == SimRuntime::OwnerType::Two;
    case SO::Three: return m_LocalPeerOwner == SimRuntime::OwnerType::Three;
    case SO::Four:  return m_LocalPeerOwner == SimRuntime::OwnerType::Four;
    case SO::Sequential:
    default:
        // single deterministic authority for SEQUENTIAL assignment
        return m_LocalPeerOwner == SimRuntime::OwnerType::One;
    }
}

void FlatBufferPreviewScenario::SendSpawnPacketForItem(const RenderItem& item, SimRuntime::SpawnerShapeType shape, float radius, float height, const glm::vec3& size)
{
    if (!m_NetworkingActive) return;

    SimSpawnPacket p{};
    p.objectId = item.objectId;
    p.owner = static_cast<uint8_t>(item.owner);
    p.shape = static_cast<uint8_t>(shape);
    strncpy_s(p.material, item.material.c_str(), _TRUNCATE);

    p.pos[0] = item.baseTransform.position.x;
    p.pos[1] = item.baseTransform.position.y;
    p.pos[2] = item.baseTransform.position.z;

    p.vel[0] = item.linearVelocity.x;
    p.vel[1] = item.linearVelocity.y;
    p.vel[2] = item.linearVelocity.z;

    p.size[0] = size.x;
    p.size[1] = size.y;
    p.size[2] = size.z;
    p.radius = radius;
    p.height = height;
    p.tick = m_NetTick;

    m_Network.SendSpawn(p);
}

void FlatBufferPreviewScenario::ReceiveRemoteSpawnPackets()
{
    if (!m_NetworkingActive) return;

    const auto packets = m_Network.ReceiveSpawns();
    for (const auto& p : packets) {
        if (FindItemById(p.objectId)) continue;

        RenderItem item{};
        item.objectId = p.objectId;
        item.name = "spawn_remote_" + std::to_string(p.objectId);
        item.material = p.material;
        item.behaviourType = SimRuntime::BehaviourType::Simulated;
        item.owner = static_cast<SimRuntime::OwnerType>(p.owner);
        item.isSimulated = true;
        item.isLocallyOwned = (item.owner == m_LocalPeerOwner);
        item.spawnedBySpawner = true;
        item.inverseMass = 1.0f;
        item.restitution = 0.45f;

        item.baseTransform.position = { p.pos[0], p.pos[1], p.pos[2] };
        item.baseTransform.scale = { 1.0f, 1.0f, 1.0f };
        item.linearVelocity = { p.vel[0], p.vel[1], p.vel[2] };

        const glm::vec3 col = ColorForIndex(p.objectId);
        const auto shape = static_cast<SimRuntime::SpawnerShapeType>(p.shape);

        switch (shape) {
        case SimRuntime::SpawnerShapeType::Sphere:
            item.boundRadius = std::max(0.05f, p.radius);
            item.mesh = MeshGenerator::GenerateSphere(item.boundRadius, 16, 12, col);
            break;
        case SimRuntime::SpawnerShapeType::Cylinder:
            item.boundRadius = std::max(std::max(0.05f, p.radius), std::max(0.10f, p.height) * 0.5f);
            item.mesh = MeshGenerator::GenerateCylinder(std::max(0.05f, p.radius), std::max(0.10f, p.height), 16, col);
            break;
        case SimRuntime::SpawnerShapeType::Capsule:
            item.boundRadius = std::max(0.05f, p.radius) + std::max(0.10f, p.height) * 0.5f;
            item.mesh = MeshGenerator::GenerateCapsule(std::max(0.05f, p.radius), std::max(0.10f, p.height), 16, 10, col);
            break;
        case SimRuntime::SpawnerShapeType::Cuboid:
        default:
        {
            glm::vec3 size = glm::max(glm::vec3(0.05f), glm::vec3(p.size[0], p.size[1], p.size[2]));
            item.boundRadius = glm::length(size * 0.5f);
            item.mesh = BuildCuboidMesh(size, col);
            break;
        }
        }

        item.model = BuildModelMatrix(item.baseTransform);
        item.initialModel = item.model;
        item.initialBaseTransform = item.baseTransform;
        item.initialLinearVelocity = item.linearVelocity;
        item.buffers = m_App->UploadMesh(item.mesh);

        m_NextObjectId = std::max(m_NextObjectId, p.objectId + 1);
        m_Items.push_back(std::move(item));
    }

    RefreshOwnershipFlagsAndStats();
}

void FlatBufferPreviewScenario::InitRuntimeSpawnersFromScene()
{
    m_RuntimeSpawners.clear();
    m_TotalSpawnedBySpawners = 0;
    m_SpawnRng.seed(1337u);

    const auto* scene = m_App->GetLoadedScene();
    if (!scene) return;

    for (const auto& s : scene->spawners) {
        SpawnerRuntime rs{};
        rs.def = s;
        m_RuntimeSpawners.push_back(std::move(rs));
    }
}

void FlatBufferPreviewScenario::ResetRuntimeSpawners()
{
    m_TotalSpawnedBySpawners = 0;
    m_SpawnRng.seed(1337u);

    for (auto& s : m_RuntimeSpawners) {
        s.singleBurstDone = false;
        s.elapsed = 0.0f;
        s.repeatAccumulator = 0.0f;
        s.spawnedCount = 0;
        s.sequentialCursor = 0;
    }
}

void FlatBufferPreviewScenario::UpdateRuntimeSpawners(float dt)
{
    if (!m_EnableRuntimeSpawners) return;

    for (auto& s : m_RuntimeSpawners) {
        s.elapsed += dt;
        if (s.elapsed < s.def.base.startTime) continue;

        // authority gate (only owner peer is allowed to spawn)
        if (!IsSpawnerAuthority(s)) continue;

        auto spawnOne = [&]() {
            RenderItem item{};
            float shapeRadius = 0.0f;
            float shapeHeight = 0.0f;
            glm::vec3 shapeSize(0.0f);
            item.objectId = m_NextObjectId++;
            item.name = s.def.base.name + "_spawn_" + std::to_string(item.objectId);
            item.material = s.def.base.material;
            item.behaviourType = SimRuntime::BehaviourType::Simulated;
            item.owner = ResolveSpawnerOwner(s);
            item.isSimulated = true;
            item.isLocallyOwned = (item.owner == m_LocalPeerOwner);
            item.spawnedBySpawner = true;

            item.baseTransform = BuildSpawnTransform(s.def.base.location);
            item.linearVelocity = RandomVec3InRange(s.def.base.linearVelocity);
            item.restitution = 0.45f;
            item.inverseMass = 1.0f;

            const glm::vec3 col = ColorForIndex(item.objectId);

            switch (s.def.shape) {
            case SimRuntime::SpawnerShapeType::Sphere:
            {
                std::uniform_real_distribution<float> rr(
                    std::min(s.def.radiusRange.min, s.def.radiusRange.max),
                    std::max(s.def.radiusRange.min, s.def.radiusRange.max));
                float r = std::max(0.05f, rr(m_SpawnRng));
                item.boundRadius = r;
                item.mesh = MeshGenerator::GenerateSphere(r, 16, 12, col);
                shapeRadius = r;
                break;
            }
            case SimRuntime::SpawnerShapeType::Cylinder:
            {
                std::uniform_real_distribution<float> rr(
                    std::min(s.def.radiusRange.min, s.def.radiusRange.max),
                    std::max(s.def.radiusRange.min, s.def.radiusRange.max));
                std::uniform_real_distribution<float> hh(
                    std::min(s.def.heightRange.min, s.def.heightRange.max),
                    std::max(s.def.heightRange.min, s.def.heightRange.max));
                float r = std::max(0.05f, rr(m_SpawnRng));
                float h = std::max(0.10f, hh(m_SpawnRng));
                item.boundRadius = std::max(r, h * 0.5f);
                item.mesh = MeshGenerator::GenerateCylinder(r, h, 16, col);
                shapeRadius = r;
                shapeHeight = h;
                break;
            }
            case SimRuntime::SpawnerShapeType::Capsule:
            {
                std::uniform_real_distribution<float> rr(
                    std::min(s.def.radiusRange.min, s.def.radiusRange.max),
                    std::max(s.def.radiusRange.min, s.def.radiusRange.max));
                std::uniform_real_distribution<float> hh(
                    std::min(s.def.heightRange.min, s.def.heightRange.max),
                    std::max(s.def.heightRange.min, s.def.heightRange.max));
                float r = std::max(0.05f, rr(m_SpawnRng));
                float h = std::max(0.10f, hh(m_SpawnRng));
                item.boundRadius = r + h * 0.5f;
                item.mesh = MeshGenerator::GenerateCapsule(r, h, 16, 10, col);
                shapeRadius = r;
                shapeHeight = h;
                break;
            }
            case SimRuntime::SpawnerShapeType::Cuboid:
            default:
            {
                glm::vec3 mn = glm::min(s.def.sizeRange.min, s.def.sizeRange.max);
                glm::vec3 mx = glm::max(s.def.sizeRange.min, s.def.sizeRange.max);
                std::uniform_real_distribution<float> sx(mn.x, mx.x);
                std::uniform_real_distribution<float> sy(mn.y, mx.y);
                std::uniform_real_distribution<float> sz(mn.z, mx.z);
                glm::vec3 size = glm::max(glm::vec3(0.05f), glm::vec3(sx(m_SpawnRng), sy(m_SpawnRng), sz(m_SpawnRng)));
                item.boundRadius = glm::length(size * 0.5f);
                item.mesh = BuildCuboidMesh(size, col);
                shapeSize = size;
                break;
            }
            }

            item.model = BuildModelMatrix(item.baseTransform);
            item.initialModel = item.model;
            item.initialBaseTransform = item.baseTransform;
            item.initialLinearVelocity = item.linearVelocity;
            item.buffers = m_App->UploadMesh(item.mesh);

            m_Items.push_back(std::move(item));
            ++s.spawnedCount;
            ++m_TotalSpawnedBySpawners;

            // send spawn event (authoritative creation distribution)
            SendSpawnPacketForItem(m_Items.back(), s.def.shape, shapeRadius, shapeHeight, shapeSize);
            };

        if (s.def.base.spawnType.mode == SimRuntime::SpawnMode::SingleBurst) {
            if (!s.singleBurstDone) {
                const uint32_t n = std::max(1u, s.def.base.spawnType.count);
                for (uint32_t i = 0; i < n; ++i) spawnOne();
                s.singleBurstDone = true;
            }
        }
        else {
            const float interval = std::max(0.001f, s.def.base.spawnType.interval);
            const uint32_t maxCount = std::max(1u, s.def.base.spawnType.maxCount);

            s.repeatAccumulator += dt;
            while (s.repeatAccumulator >= interval && s.spawnedCount < maxCount) {
                s.repeatAccumulator -= interval;
                spawnOne();
            }
        }
    }

    RefreshOwnershipFlagsAndStats();
}

void FlatBufferPreviewScenario::BuildFromLoadedScene() {
    const auto* scene = m_App->GetLoadedScene();
    if (!scene) {
        BuildFallbackScene();
        return;
    }

    for (size_t i = 0; i < scene->objects.size(); ++i) {
        const auto& obj = scene->objects[i];
        RenderItem item{};
        item.objectId = m_NextObjectId++;
        item.name = obj.name;
        item.material = obj.material;
        item.behaviourType = obj.behaviourType;
        item.owner = obj.owner;
        item.model = BuildModelMatrix(obj.transform);

        item.baseTransform = obj.transform;
        if (obj.behaviourType == SimRuntime::BehaviourType::Simulated) {
            item.isSimulated = true;
            item.linearVelocity = obj.initialState.linearVelocity;
            item.inverseMass = 1.0f; // placeholder until density/mass calc integration
            item.restitution = 0.45f;
        }

        if (obj.behaviourType == SimRuntime::BehaviourType::Animated) {
            item.waypoints = obj.waypoints;
            item.totalDuration = obj.totalDuration;
            item.easing = obj.easing;
            item.pathMode = obj.pathMode;
            item.animTime = 0.0f;
            item.reverse = false;

            if (item.waypoints.size() < 2) {
                m_LocalWarnings.push_back("Animated object '" + item.name + "' has <2 waypoints, using static transform.");
            }
        }

        const glm::vec3 c = ColorForIndex(i);

        switch (obj.shapeType) {
        case SimRuntime::ShapeType::Sphere:
            item.boundRadius = std::max(0.05f, obj.radius);
            item.mesh = MeshGenerator::GenerateSphere(std::max(0.05f, obj.radius), 24, 16, c);
            break;
        case SimRuntime::ShapeType::Cylinder:
            item.boundRadius = std::max(std::max(0.05f, obj.radius), std::max(0.05f, obj.height) * 0.5f);
            item.mesh = MeshGenerator::GenerateCylinder(std::max(0.05f, obj.radius), std::max(0.05f, obj.height), 24, c);
            break;

        case SimRuntime::ShapeType::Capsule:
            item.boundRadius = std::max(0.05f, obj.radius) + std::max(0.10f, obj.height) * 0.5f;
            item.mesh = MeshGenerator::GenerateCapsule(std::max(0.05f, obj.radius), std::max(0.10f, obj.height), 24, 12, c);
            break;
        case SimRuntime::ShapeType::Plane:
            item.isPlane = true;
            item.mesh = MeshGenerator::GeneratePlane(10.0f, 10.0f, 10, 10, c);
            if (glm::length(obj.planeNormal) > 0.0001f) {
                glm::vec3 n = glm::normalize(obj.planeNormal);
                glm::quat q = glm::rotation(glm::vec3(0, 1, 0), n);
                item.model = item.model * glm::mat4_cast(q);
            }
            break;
        case SimRuntime::ShapeType::Cuboid:
        {
            const glm::vec3 clamped = glm::max(obj.size, glm::vec3(0.05f));
            item.boundRadius = glm::length(clamped * 0.5f);
            item.mesh = BuildCuboidMesh(clamped, c);
            break;
        }
        default:
            ++m_UnsupportedCount;
            m_LocalWarnings.push_back("Skipped unsupported shape on object: " + obj.name);
            continue;
        }

        item.buffers = m_App->UploadMesh(item.mesh);
        item.initialModel = item.model;
        item.initialBaseTransform = item.baseTransform;
        item.initialLinearVelocity = item.linearVelocity;
        m_Items.push_back(std::move(item));
    }
}

void FlatBufferPreviewScenario::BuildFallbackScene() {
    m_UsingFallbackData = true;

    {
        RenderItem s{};
        s.objectId = m_NextObjectId++;
        s.name = "fallback sphere";
        s.material = "default";
        s.behaviourType = SimRuntime::BehaviourType::Animated;
        s.baseTransform = {};
        s.baseTransform.position = glm::vec3(-1.5f, 1.0f, 0.0f);
        s.waypoints = {
            { {-3.0f, 1.0f, 0.0f}, {0,0,0}, 0.0f },
            { { 0.0f, 2.2f, 0.0f}, {0,0,0}, 1.5f },
            { { 3.0f, 1.0f, 0.0f}, {0,0,0}, 3.0f }
        };
        s.totalDuration = 4.5f;
        s.easing = SimRuntime::EasingType::SmoothStep;
        s.pathMode = SimRuntime::PathMode::Loop;
        s.model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.5f, 1.0f, 0.0f));
        s.mesh = MeshGenerator::GenerateSphere(0.8f, 24, 16, { 1.0f, 0.35f, 0.35f });
        s.buffers = m_App->UploadMesh(s.mesh);
        s.initialModel = s.model;
        s.initialBaseTransform = s.baseTransform;
        s.initialLinearVelocity = s.linearVelocity;
        m_Items.push_back(std::move(s));
    }

    {
        RenderItem b{};
        b.objectId = m_NextObjectId++;
        b.name = "fallback cuboid";
        b.material = "default";

        // Simulated ownership test object (set BEFORE push_back)
        b.behaviourType = SimRuntime::BehaviourType::Simulated;
        b.owner = SimRuntime::OwnerType::Two;
        b.isSimulated = true;
        b.inverseMass = 1.0f;
        b.linearVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        b.restitution = 0.35f;

        const glm::vec3 cuboidSize{ 1.4f, 1.4f, 1.4f };
        b.boundRadius = glm::length(cuboidSize * 0.5f);

        b.baseTransform = {};
        b.baseTransform.position = glm::vec3(1.5f, 3.0f, 0.0f);
        b.model = BuildModelMatrix(b.baseTransform);

        b.mesh = BuildCuboidMesh(cuboidSize, { 0.3f, 0.8f, 1.0f });
        b.buffers = m_App->UploadMesh(b.mesh);

        b.initialModel = b.model;
        b.initialBaseTransform = b.baseTransform;
        b.initialLinearVelocity = b.linearVelocity;

        m_Items.push_back(std::move(b));
    }

    {
        RenderItem bs{};
        bs.objectId = m_NextObjectId++;
        bs.name = "fallback bouncer sphere";
        bs.material = "default";
        bs.behaviourType = SimRuntime::BehaviourType::Simulated;
        bs.owner = SimRuntime::OwnerType::Two;
        bs.isSimulated = true;
        bs.inverseMass = 1.0f;
        bs.restitution = 1.0f;
        bs.boundRadius = 0.6f;

        bs.baseTransform = {};
        bs.baseTransform.position = glm::vec3(-2.8f, 3.5f, 0.0f);
        bs.linearVelocity = glm::vec3(0.0f, 0.0f, 0.0f);
        bs.model = BuildModelMatrix(bs.baseTransform);

        bs.mesh = MeshGenerator::GenerateSphere(0.6f, 24, 16, { 1.0f, 0.9f, 0.2f });
        bs.buffers = m_App->UploadMesh(bs.mesh);

        bs.initialModel = bs.model;
        bs.initialBaseTransform = bs.baseTransform;
        bs.initialLinearVelocity = bs.linearVelocity;

        m_Items.push_back(std::move(bs));
    }

    {
        RenderItem p{};
        p.objectId = m_NextObjectId++;
        p.name = "fallback plane";
        p.material = "default";
        p.isPlane = true;
        p.model = glm::mat4(1.0f);
        p.mesh = MeshGenerator::GeneratePlane(12.0f, 12.0f, 12, 12, { 0.45f, 0.45f, 0.45f });
        p.buffers = m_App->UploadMesh(p.mesh);
        p.initialModel = p.model;
        p.initialBaseTransform = p.baseTransform;
        p.initialLinearVelocity = p.linearVelocity;
        m_Items.push_back(std::move(p));
    }
}

void FlatBufferPreviewScenario::OnLoad() {
    Clear();
    m_UsingFallbackData = false;
    m_NextObjectId = 1;
    m_NetTick = 0;
    BuildFromLoadedScene();
    InitRuntimeSpawnersFromScene();
    RefreshOwnershipFlagsAndStats();
    StartNetworkWorker();
}

void FlatBufferPreviewScenario::OnUpdate(float deltaTime) {
    auto ease = [](float t, SimRuntime::EasingType easingType) {
        t = std::clamp(t, 0.0f, 1.0f);
        if (easingType == SimRuntime::EasingType::SmoothStep) {
            return t * t * (3.0f - 2.0f * t);
        }
        return t;
        };

    auto sampleAt = [&](const RenderItem& item, float absoluteTime) -> SimRuntime::Transform {
        SimRuntime::Transform out = item.baseTransform;

        if (item.waypoints.empty()) {
            return out;
        }
        if (item.waypoints.size() == 1) {
            out.position = item.waypoints[0].position;
            out.orientation = item.waypoints[0].rotation;
            return out;
        }

        if (absoluteTime <= item.waypoints.front().time) {
            out.position = item.waypoints.front().position;
            out.orientation = item.waypoints.front().rotation;
            return out;
        }

        if (absoluteTime >= item.waypoints.back().time) {
            out.position = item.waypoints.back().position;
            out.orientation = item.waypoints.back().rotation;
            return out;
        }

        for (size_t i = 0; i + 1 < item.waypoints.size(); ++i) {
            const auto& a = item.waypoints[i];
            const auto& b = item.waypoints[i + 1];
            if (absoluteTime >= a.time && absoluteTime <= b.time) {
                const float segDt = std::max(0.0001f, b.time - a.time);
                float t = (absoluteTime - a.time) / segDt;
                t = ease(t, item.easing);

                out.position = glm::mix(a.position, b.position, t);
                out.orientation.yaw = glm::mix(a.rotation.yaw, b.rotation.yaw, t);
                out.orientation.pitch = glm::mix(a.rotation.pitch, b.rotation.pitch, t);
                out.orientation.roll = glm::mix(a.rotation.roll, b.rotation.roll, t);
                return out;
            }
        }

        return out;
        };

    std::lock_guard<std::mutex> lock(m_ItemsMutex);

    UpdateRuntimeSpawners(deltaTime);

    for (auto& item : m_Items) {
        if (item.behaviourType != SimRuntime::BehaviourType::Animated || item.waypoints.size() < 2) {
            continue;
        }

        const float lastWpTime = item.waypoints.back().time;
        const float pathEnd = std::max(lastWpTime, 0.0001f);

        switch (item.pathMode) {
        case SimRuntime::PathMode::Stop:
        {
            item.animTime = std::min(item.animTime + deltaTime, pathEnd);
            const SimRuntime::Transform t = sampleAt(item, item.animTime);
            item.model = BuildModelMatrix(t);
            break;
        }
        case SimRuntime::PathMode::Loop:
        {
            const float loopDuration = std::max(item.totalDuration, pathEnd);
            item.animTime += deltaTime;
            if (item.animTime > loopDuration) {
                item.animTime = std::fmod(item.animTime, loopDuration);
            }

            float sampleTime = item.animTime;
            if (sampleTime > pathEnd && loopDuration > pathEnd) {
                const float alpha = (sampleTime - pathEnd) / (loopDuration - pathEnd);
                const auto& first = item.waypoints.front();
                const auto& last = item.waypoints.back();
                SimRuntime::Transform t = item.baseTransform;
                t.position = glm::mix(last.position, first.position, alpha);
                t.orientation.yaw = glm::mix(last.rotation.yaw, first.rotation.yaw, alpha);
                t.orientation.pitch = glm::mix(last.rotation.pitch, first.rotation.pitch, alpha);
                t.orientation.roll = glm::mix(last.rotation.roll, first.rotation.roll, alpha);
                item.model = BuildModelMatrix(t);
            }
            else {
                const SimRuntime::Transform t = sampleAt(item, sampleTime);
                item.model = BuildModelMatrix(t);
            }
            break;
        }
        case SimRuntime::PathMode::Reverse:
        {
            if (!item.reverse) {
                item.animTime += deltaTime;
                if (item.animTime >= pathEnd) {
                    item.animTime = pathEnd;
                    item.reverse = true;
                }
            }
            else {
                item.animTime -= deltaTime;
                if (item.animTime <= 0.0f) {
                    item.animTime = 0.0f;
                    item.reverse = false;
                }
            }

            const SimRuntime::Transform t = sampleAt(item, item.animTime);
            item.model = BuildModelMatrix(t);
            break;
        }
        default:
            break;
        }
    }

    // Remote object drift correction / interpolation
    for (auto& item : m_Items) {
        if (!item.isSimulated || item.isLocallyOwned || !item.hasReplicatedState) {
            continue;
        }

        const glm::vec3 toTarget = item.replicatedTargetPos - item.baseTransform.position;
        const float dist = glm::length(toTarget);

        if (dist > m_RemoteSnapDistance) {
            item.baseTransform.position = item.replicatedTargetPos;
            item.linearVelocity = item.replicatedTargetVel;
        }
        else {
            const float alpha = 1.0f - std::exp(-m_RemoteInterpRate * deltaTime);
            item.baseTransform.position = glm::mix(item.baseTransform.position, item.replicatedTargetPos, alpha);
            item.linearVelocity = glm::mix(item.linearVelocity, item.replicatedTargetVel, alpha);
        }

        item.model = BuildModelMatrix(item.baseTransform);
    }

    const float gravity = -9.81f;
    const float groundY = 0.0f;

    for (auto& item : m_Items) {
        if (!item.isSimulated || !item.isLocallyOwned || item.inverseMass <= 0.0f) {
            continue; // owner-driven: only local owner simulates
        }

        item.linearVelocity.y += gravity * deltaTime;
        item.baseTransform.position += item.linearVelocity * deltaTime;

        const float minY = groundY + item.boundRadius;
        if (item.baseTransform.position.y < minY) {
            item.baseTransform.position.y = minY;
            if (item.linearVelocity.y < 0.0f) {
                item.linearVelocity.y = -item.linearVelocity.y * item.restitution;
                item.linearVelocity.x *= 0.90f;
                item.linearVelocity.z *= 0.90f;
            }
        }

        item.model = BuildModelMatrix(item.baseTransform);
    }
}

void FlatBufferPreviewScenario::OnRender(VkCommandBuffer commandBuffer) {
    std::lock_guard<std::mutex> lock(m_ItemsMutex);

    auto& material = m_App->GetMaterialSettings();

    auto drawItem = [&](const RenderItem& item) {
        PushConstants push{};
        push.model = item.model;

        glm::vec4 tint = (m_DisplayMode == DisplayMode::Owner)
            ? OwnerColor(item)
            : MaterialColor(item);

        push.checkerColorA = tint;
        push.checkerColorB = glm::mix(tint, glm::vec4(0.08f, 0.08f, 0.08f, 1.0f), 0.65f);
        push.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

        vkCmdPushConstants(commandBuffer, m_App->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &push);

        VkBuffer vertexBuffers[] = { item.buffers.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, item.buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, item.buffers.indexCount, 1, 0, 0, 0);
        };

    // Pass 1: planes
    for (const auto& item : m_Items) {
        if (item.isPlane) drawItem(item);
    }

    // Pass 2: everything else
    for (const auto& item : m_Items) {
        if (!item.isPlane) drawItem(item);
    }
}

void FlatBufferPreviewScenario::OnUnload() {
    StopNetworkWorker();

    if (m_NetworkingActive) {
        m_Network.Shutdown();
        m_NetworkingActive = false;
    }
    Clear();
}

void FlatBufferPreviewScenario::OnImGui() {
    ImGui::Begin("FlatBuffer Preview");

    ReceiveAndApplyRemoteCommands();

    const auto* scene = m_App->GetLoadedScene();
    if (scene) {
        ImGui::Text("Loaded Scene: %s", scene->name.c_str());
        ImGui::Text("Objects: %d", static_cast<int>(scene->objects.size()));
        ImGui::Text("Spawners: %d", static_cast<int>(scene->spawners.size()));
        ImGui::Text("Materials: %d", static_cast<int>(scene->materials.size()));
    } else {
        ImGui::Text("No scene.bin loaded");
    }

    if (m_UsingFallbackData) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Using fallback preview geometry");
    }

    ImGui::Text("Rendered Items: %d", static_cast<int>(m_Items.size()));
    ImGui::Text("Unsupported Objects Skipped: %d", m_UnsupportedCount);

    int mode = static_cast<int>(m_DisplayMode);
    ImGui::RadioButton("Owner Colours", &mode, static_cast<int>(DisplayMode::Owner));
    ImGui::SameLine();
    ImGui::RadioButton("Material Colours", &mode, static_cast<int>(DisplayMode::Material));
    m_DisplayMode = static_cast<DisplayMode>(mode);

    const auto& loadWarnings = m_App->GetLoadedSceneWarnings();
    if (!loadWarnings.empty() && ImGui::TreeNode("Loader Warnings")) {
        for (const auto& w : loadWarnings) {
            ImGui::BulletText("%s", w.c_str());
        }
        ImGui::TreePop();
    }

    if (!m_LocalWarnings.empty() && ImGui::TreeNode("Preview Warnings")) {
        for (const auto& w : m_LocalWarnings) {
            ImGui::BulletText("%s", w.c_str());
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::Text("Spawner Runtime");
    ImGui::Checkbox("Enable Runtime Spawners", &m_EnableRuntimeSpawners);
    ImGui::Text("Loaded Spawners: %d", static_cast<int>(m_RuntimeSpawners.size()));
    ImGui::Text("Spawned Objects Total: %u", m_TotalSpawnedBySpawners);

    const char* peerNames[] = { "ONE", "TWO", "THREE", "FOUR" };
    int localPeer = static_cast<int>(m_LocalPeerOwner);
    if (ImGui::Combo("Local Peer (Ownership View)", &localPeer, peerNames, IM_ARRAYSIZE(peerNames))) {
        m_LocalPeerOwner = static_cast<SimRuntime::OwnerType>(localPeer);
        RefreshOwnershipFlagsAndStats();
    }
     
    ImGui::Text("Local-owned simulated: %d", m_LocalOwnedSimulatedCount);
    ImGui::Text("Remote simulated: %d", m_RemoteSimulatedCount);

    ImGui::Separator();
    ImGui::Text("Networking (UDP P2P Baseline)");
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

    ImGui::Text("Network Status: %s", m_NetworkingActive ? "ACTIVE" : "INACTIVE");

    ImGui::SliderFloat("Network Tick Hz", &m_NetworkTargetHz, 1.0f, 120.0f, "%.1f");
    ImGui::Text("Measured Network Hz: %.1f", m_NetworkMeasuredHz.load());

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

    ImGui::Separator();
    ImGui::Text("Global Command Replication");

    if (ImGui::Button("Global Play")) {
        m_App->Play();
        SendGlobalCommand(NetCommandType::Play);
    }
    ImGui::SameLine();
    if (ImGui::Button("Global Pause")) {
        m_App->Pause();
        SendGlobalCommand(NetCommandType::Pause);
    }
    ImGui::SameLine();
    if (ImGui::Button("Global Reset")) {
        SendGlobalCommand(NetCommandType::Reset);
        ResetRuntimeState();
    }

    float ts = m_App->GetTimeStep();
    if (ImGui::SliderFloat("Global TimeStep", &ts, 0.001f, 0.05f, "%.4f s")) {
        m_App->SetTimeStep(ts);
        SendGlobalCommand(NetCommandType::SetTimeStep, ts);
    }

    float sp = m_App->GetSimulationSpeed();
    if (ImGui::SliderFloat("Global Speed", &sp, 0.0f, 4.0f, "%.2fx")) {
        m_App->SetSimulationSpeed(sp);
        SendGlobalCommand(NetCommandType::SetSpeed, sp);
    }

    ImGui::End();
}

glm::vec4 FlatBufferPreviewScenario::OwnerColor(const RenderItem& item) const {
    if (item.behaviourType != SimRuntime::BehaviourType::Simulated) {
        return { 0.7f, 0.7f, 0.7f, 1.0f };
    }

    switch (item.owner) {
    case SimRuntime::OwnerType::One:   return { 1.0f, 0.2f, 0.2f, 1.0f }; // red
    case SimRuntime::OwnerType::Two:   return { 0.2f, 1.0f, 0.2f, 1.0f }; // green
    case SimRuntime::OwnerType::Three: return { 0.2f, 0.4f, 1.0f, 1.0f }; // blue
    case SimRuntime::OwnerType::Four:  return { 1.0f, 1.0f, 0.2f, 1.0f }; // yellow
    default:                           return { 1.0f, 1.0f, 1.0f, 1.0f };
    }
}

glm::vec4 FlatBufferPreviewScenario::MaterialColor(const RenderItem& item) const {
    // deterministic simple hash color
    uint32_t h = 2166136261u;
    for (char c : item.material) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }

    const float r = 0.25f + ((h & 0xFF) / 255.0f) * 0.75f;
    const float g = 0.25f + (((h >> 8) & 0xFF) / 255.0f) * 0.75f;
    const float b = 0.25f + (((h >> 16) & 0xFF) / 255.0f) * 0.75f;
    return { r, g, b, 1.0f };
}

void FlatBufferPreviewScenario::RefreshOwnershipFlagsAndStats()
{
    m_LocalOwnedSimulatedCount = 0;
    m_RemoteSimulatedCount = 0;

    for (auto& item : m_Items) {
        if (item.behaviourType == SimRuntime::BehaviourType::Simulated) {
            item.isLocallyOwned = (item.owner == m_LocalPeerOwner);
            if (item.isLocallyOwned) {
                ++m_LocalOwnedSimulatedCount;
            }
            else {
                ++m_RemoteSimulatedCount;
            }
        }
        else {
            item.isLocallyOwned = true;
        }
    }
}

FlatBufferPreviewScenario::RenderItem* FlatBufferPreviewScenario::FindItemById(uint32_t id)
{
    for (auto& item : m_Items) {
        if (item.objectId == id) return &item;
    }
    return nullptr;
}

void FlatBufferPreviewScenario::SendOwnedSimulatedStates()
{
    if (!m_NetworkingActive) return;

    for (const auto& item : m_Items) {
        if (!item.isSimulated || !item.isLocallyOwned) continue;

        SimStatePacket p{};
        p.objectId = item.objectId;
        p.owner = static_cast<uint8_t>(item.owner);
        p.pos[0] = item.baseTransform.position.x;
        p.pos[1] = item.baseTransform.position.y;
        p.pos[2] = item.baseTransform.position.z;
        p.vel[0] = item.linearVelocity.x;
        p.vel[1] = item.linearVelocity.y;
        p.vel[2] = item.linearVelocity.z;
        p.tick = m_NetTick;

        m_Network.SendState(p);
    }

    ++m_NetTick;
}

void FlatBufferPreviewScenario::ReceiveRemoteSimulatedStates(float dt)
{
    if (!m_NetworkingActive) return;

    auto applyPacket = [this](const SimStatePacket& p) {
        RenderItem* item = FindItemById(p.objectId);
        if (!item) return;
        if (!item->isSimulated) return;
        if (item->isLocallyOwned) return;

        item->replicatedTargetPos = { p.pos[0], p.pos[1], p.pos[2] };
        item->replicatedTargetVel = { p.vel[0], p.vel[1], p.vel[2] };
        item->hasReplicatedState = true;
        };

    const bool emulate = m_EnableNetEmulation.load();
    const float baseMs = m_EmuBaseLatencyMs.load();
    const float jitterMs = m_EmuJitterMs.load();
    const float lossPct = m_EmuLossPercent.load();

    std::uniform_real_distribution<float> lossDist(0.0f, 100.0f);
    std::uniform_real_distribution<float> jitterDist(-jitterMs, jitterMs);

    const auto packets = m_Network.ReceiveStates();
    for (const auto& p : packets) {
        if (emulate) {
            if (lossDist(m_NetRng) < lossPct) {
                continue; // dropped
            }

            const float delayedMs = std::max(0.0f, baseMs + jitterDist(m_NetRng));
            DelayedStatePacket delayed{};
            delayed.packet = p;
            delayed.remainingDelaySec = delayedMs * 0.001f;
            m_DelayedIncomingStates.push_back(delayed);
        }
        else {
            applyPacket(p);
        }
    }

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

void FlatBufferPreviewScenario::SendGlobalCommand(NetCommandType command, float value)
{
    if (!m_NetworkingActive) return;

    SimCommandPacket p{};
    p.command = command;
    p.value = value;
    p.tick = m_NetTick;
    m_Network.SendCommand(p);
}

void FlatBufferPreviewScenario::ReceiveAndApplyRemoteCommands()
{
    if (!m_NetworkingActive) return;

    const auto commands = m_Network.ReceiveCommands();
    for (const auto& c : commands) {
        switch (c.command) {
        case NetCommandType::Play:
            m_App->Play();
            break;
        case NetCommandType::Pause:
            m_App->Pause();
            break;
        case NetCommandType::Reset:
            ResetRuntimeState();
            break;
        case NetCommandType::SetTimeStep:
            m_App->SetTimeStep(std::max(0.0005f, c.value));
            break;
        case NetCommandType::SetSpeed:
            m_App->SetSimulationSpeed(std::max(0.0f, c.value));
            break;
        default:
            break;
        }
    }
}

void FlatBufferPreviewScenario::StartNetworkWorker()
{
    if (m_RunNetworkThread.exchange(true)) {
        return;
    }

    m_DelayedIncomingStates.clear();

    m_NetworkThread = std::thread([this]() { NetworkWorkerMain(); });

#ifdef _WIN32
    if (const DWORD_PTR netMask = GetNetworkingAffinityMask(); netMask != 0) {
        SetThreadAffinityMask(
            reinterpret_cast<HANDLE>(m_NetworkThread.native_handle()),
            netMask
        );
    }
#endif
}

void FlatBufferPreviewScenario::StopNetworkWorker()
{
    if (!m_RunNetworkThread.exchange(false)) {
        return;
    }

    if (m_NetworkThread.joinable()) {
        m_NetworkThread.join();
    }
}

void FlatBufferPreviewScenario::NetworkWorkerMain()
{
    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (m_RunNetworkThread.load()) {
        const float hz = std::max(1.0f, m_NetworkTargetHz);
        const auto step = std::chrono::duration<float>(1.0f / hz);

        auto now = clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        {
            std::lock_guard<std::mutex> lock(m_ItemsMutex);
            ReceiveRemoteSpawnPackets();
            ReceiveRemoteSimulatedStates(dt);
            SendOwnedSimulatedStates();
        }

        if (dt > 0.0001f) {
            m_NetworkMeasuredHz.store(1.0f / dt);
        }

        std::this_thread::sleep_for(step);
    }
}

void FlatBufferPreviewScenario::ResetRuntimeState()
{
    std::lock_guard<std::mutex> lock(m_ItemsMutex);

    for (auto& item : m_Items) {
        item.model = item.initialModel;
        item.baseTransform = item.initialBaseTransform;
        item.linearVelocity = item.initialLinearVelocity;

        item.animTime = 0.0f;
        item.reverse = false;

        item.hasReplicatedState = false;
        item.replicatedTargetPos = item.baseTransform.position;
        item.replicatedTargetVel = item.linearVelocity;
    }

    for (size_t i = 0; i < m_Items.size(); ) {
        if (m_Items[i].spawnedBySpawner) {
            m_App->DestroyMeshBuffers(m_Items[i].buffers);
            m_Items.erase(m_Items.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else {
            ++i;
        }
    }

    ResetRuntimeSpawners();
    RefreshOwnershipFlagsAndStats();

    m_DelayedIncomingStates.clear();
    m_NetTick = 0;
}
