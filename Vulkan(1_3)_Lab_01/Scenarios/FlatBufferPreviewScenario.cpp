#include "FlatBufferPreviewScenario.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

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

        m_Items.push_back(std::move(b));
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
        m_Items.push_back(std::move(p));
    }
}

void FlatBufferPreviewScenario::OnLoad() {
    Clear();
    m_UsingFallbackData = false;
    m_NextObjectId = 1;
    m_NetTick = 0;
    BuildFromLoadedScene();
    RefreshOwnershipFlagsAndStats();
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

    ReceiveRemoteSimulatedStates();

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

    SendOwnedSimulatedStates();
}

void FlatBufferPreviewScenario::OnRender(VkCommandBuffer commandBuffer) {
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
        m_App->Stop();
        SendGlobalCommand(NetCommandType::Reset);
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

void FlatBufferPreviewScenario::ReceiveRemoteSimulatedStates()
{
    if (!m_NetworkingActive) return;

    const auto packets = m_Network.ReceiveStates();
    for (const auto& p : packets) {
        RenderItem* item = FindItemById(p.objectId);
        if (!item) continue;
        if (!item->isSimulated) continue;
        if (item->isLocallyOwned) continue; // local authority wins

        item->baseTransform.position = { p.pos[0], p.pos[1], p.pos[2] };
        item->linearVelocity = { p.vel[0], p.vel[1], p.vel[2] };
        item->model = BuildModelMatrix(item->baseTransform);
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
            m_App->Stop();
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