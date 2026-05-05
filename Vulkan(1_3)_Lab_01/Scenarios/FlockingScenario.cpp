#include "FlockingScenario.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>

#ifdef _WIN32
namespace
{
    DWORD_PTR GetNetworkingAffinityMask()
    {
        const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (count == 0) return 0;

        DWORD_PTR mask = 0;
        if (count > 1) mask |= (static_cast<DWORD_PTR>(1) << 1);
        if (count > 2) mask |= (static_cast<DWORD_PTR>(1) << 2);

        if (mask == 0) mask = (static_cast<DWORD_PTR>(1) << 0);
        return mask;
    }
}
#endif

void FlockingScenario::OnLoad()
{
    std::lock_guard<std::mutex> lock(m_BoidsMutex);

    m_Boids.clear();
    m_NextBoidId = 1;
    m_NetTick = 0;
    m_LocalOwnedCount = 0;
    m_RemoteCount = 0;

    m_BoidMesh = MeshGenerator::GenerateSphere(m_BoidRadius, 16, 12, { 0.9f, 0.9f, 1.0f });
    m_BoidBuffers = m_App->UploadMesh(m_BoidMesh);

    BuildFromSceneOrFallback();
    RefreshOwnershipFlags();
    StartNetworkWorker();

    m_TxPackets.store(0);
    m_RxPackets.store(0);
}

void FlockingScenario::OnUnload()
{
    StopNetworkWorker();

    if (m_NetworkingActive) {
        m_Network.Shutdown();
        m_NetworkingActive = false;
    }

    std::lock_guard<std::mutex> lock(m_BoidsMutex);
    m_App->DestroyMeshBuffers(m_BoidBuffers);
    m_Boids.clear();
}

void FlockingScenario::BuildFromSceneOrFallback()
{
    const auto* scene = m_App->GetLoadedScene();
    if (scene) {
        m_Settings = scene->flocking;
    } else {
        m_Settings = {};
        m_Settings.enabled = true;
    }

    if (m_Settings.boidCount == 0) m_Settings.boidCount = 40;
    BuildBoids(m_Settings.boidCount, m_Settings.spawnCenter, m_Settings.spawnExtents);
}

void FlockingScenario::BuildBoids(uint32_t count, const glm::vec3& center, const glm::vec3& extents)
{
    std::uniform_real_distribution<float> dx(-extents.x, extents.x);
    std::uniform_real_distribution<float> dy(-extents.y, extents.y);
    std::uniform_real_distribution<float> dz(-extents.z, extents.z);
    std::uniform_real_distribution<float> vv(-1.0f, 1.0f);

    m_Boids.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Boid b{};
        b.id = m_NextBoidId++;
        b.owner = m_FlockOwner;
        b.position = center + glm::vec3(dx(m_Rng), dy(m_Rng), dz(m_Rng));
        b.velocity = glm::normalize(glm::vec3(vv(m_Rng), vv(m_Rng), vv(m_Rng))) * (m_Settings.maxSpeed * 0.35f);
        b.model = glm::translate(glm::mat4(1.0f), b.position);
        b.initialPosition = b.position;
        b.initialVelocity = b.velocity;
        m_Boids.push_back(b);
    }
}

void FlockingScenario::ResetBoids_NoLock()
{
    for (auto& b : m_Boids) {
        b.position = b.initialPosition;
        b.velocity = b.initialVelocity;
        b.model = glm::translate(glm::mat4(1.0f), b.position);
        b.hasReplicatedState = false;
        b.replicatedTargetPos = b.position;
        b.replicatedTargetVel = b.velocity;
    }
    m_NetTick = 0;
}

void FlockingScenario::ResetBoids()
{
    std::lock_guard<std::mutex> lock(m_BoidsMutex);
    ResetBoids_NoLock();
}

void FlockingScenario::RefreshOwnershipFlags()
{
    m_LocalOwnedCount = 0;
    m_RemoteCount = 0;
    for (auto& b : m_Boids) {
        b.owner = m_FlockOwner;
        b.isLocallyOwned = (b.owner == m_LocalPeerOwner);
        if (b.isLocallyOwned) ++m_LocalOwnedCount; else ++m_RemoteCount;
    }
}

FlockingScenario::Boid* FlockingScenario::FindBoidById(uint32_t id)
{
    for (auto& b : m_Boids) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

// REPLACE ComputeSteering with this version

glm::vec3 FlockingScenario::ComputeSteering(size_t i) const
{
    const Boid& self = m_Boids[i];

    glm::vec3 cohesion(0.0f), alignment(0.0f), separation(0.0f);
    int nCohAlign = 0, nSep = 0;

    auto accumulateNeighbor = [&](size_t j) {
        if (i == j) return;
        const Boid& other = m_Boids[j];

        const glm::vec3 d = other.position - self.position;
        const float dist = glm::length(d);
        if (dist <= 0.0001f) return;

        if (dist < m_Settings.neighborRadius) {
            cohesion += other.position;
            alignment += other.velocity;
            ++nCohAlign;
        }

        if (dist < m_Settings.separationRadius) {
            separation -= (d / dist) / std::max(0.1f, dist);
            ++nSep;
        }
        };

    if (m_SearchMode == NeighborSearchMode::UniformGrid) {
        std::vector<size_t> candidates;
        CollectUniformGridCandidates(i, candidates);
        for (size_t j : candidates) {
            accumulateNeighbor(j);
        }
    }
    else if (m_SearchMode == NeighborSearchMode::Octree) {
        std::vector<size_t> candidates;
        CollectOctreeCandidates(m_OctreeRoot.get(), self.position, m_Settings.neighborRadius, candidates);
        for (size_t j : candidates) {
            accumulateNeighbor(j);
        }
    }
    else {
        for (size_t j = 0; j < m_Boids.size(); ++j) {
            accumulateNeighbor(j);
        }
    }

    glm::vec3 force(0.0f);

    if (nCohAlign > 0) {
        cohesion /= static_cast<float>(nCohAlign);
        glm::vec3 desiredC = cohesion - self.position;
        if (glm::length(desiredC) > 0.0001f) {
            desiredC = glm::normalize(desiredC) * m_Settings.maxSpeed;
            force += (desiredC - self.velocity) * m_Settings.weightCohesion;
        }

        alignment /= static_cast<float>(nCohAlign);
        if (glm::length(alignment) > 0.0001f) {
            glm::vec3 desiredA = glm::normalize(alignment) * m_Settings.maxSpeed;
            force += (desiredA - self.velocity) * m_Settings.weightAlignment;
        }
    }

    if (nSep > 0 && glm::length(separation) > 0.0001f) {
        glm::vec3 desiredS = glm::normalize(separation) * m_Settings.maxSpeed;
        force += (desiredS - self.velocity) * m_Settings.weightSeparation;
    }

    const glm::vec3 toCenter = self.position - m_Settings.spawnCenter;
    glm::vec3 avoidance(0.0f);
    if (std::abs(toCenter.x) > m_Settings.spawnExtents.x - m_Settings.avoidanceRadius) avoidance.x = -toCenter.x;
    if (std::abs(toCenter.y) > m_Settings.spawnExtents.y - m_Settings.avoidanceRadius) avoidance.y = -toCenter.y;
    if (std::abs(toCenter.z) > m_Settings.spawnExtents.z - m_Settings.avoidanceRadius) avoidance.z = -toCenter.z;

    if (glm::length(avoidance) > 0.0001f) {
        glm::vec3 desiredV = glm::normalize(avoidance) * m_Settings.maxSpeed;
        force += (desiredV - self.velocity) * m_Settings.weightAvoidance;
    }

    const float fLen = glm::length(force);
    if (fLen > m_Settings.maxForce) {
        force = (force / fLen) * m_Settings.maxForce;
    }

    return force;
}

void FlockingScenario::OnUpdate(float dt)
{
    std::lock_guard<std::mutex> lock(m_BoidsMutex);
    const auto t0 = std::chrono::steady_clock::now();

    if (m_SearchMode == NeighborSearchMode::UniformGrid) {
        const auto g0 = std::chrono::steady_clock::now();
        BuildUniformGrid();
        const auto g1 = std::chrono::steady_clock::now();
        m_LastGridBuildMs = std::chrono::duration<float, std::milli>(g1 - g0).count();
        m_LastOctreeBuildMs = 0.0f;
        m_LastOctreeNodeCount = 0;
        m_OctreeRoot.reset();
    }
    else if (m_SearchMode == NeighborSearchMode::Octree) {
        const auto o0 = std::chrono::steady_clock::now();
        BuildOctree();
        const auto o1 = std::chrono::steady_clock::now();
        m_LastOctreeBuildMs = std::chrono::duration<float, std::milli>(o1 - o0).count();

        m_UniformGrid.clear();
        m_LastGridBuildMs = 0.0f;
        m_LastGridCellCount = 0;
        m_LastGridEntryCount = 0;
    }
    else {
        m_UniformGrid.clear();
        m_OctreeRoot.reset();
        m_LastGridBuildMs = 0.0f;
        m_LastOctreeBuildMs = 0.0f;
        m_LastGridCellCount = 0;
        m_LastGridEntryCount = 0;
        m_LastOctreeNodeCount = 0;
    }

    // local simulation
    for (size_t i = 0; i < m_Boids.size(); ++i) {
        Boid& b = m_Boids[i];
        if (!b.isLocallyOwned) continue;

        const glm::vec3 accel = ComputeSteering(i);
        b.velocity += accel * dt;

        const float speed = glm::length(b.velocity);
        if (speed > m_Settings.maxSpeed) {
            b.velocity = (b.velocity / speed) * m_Settings.maxSpeed;
        }

        b.position += b.velocity * dt;
        b.model = glm::translate(glm::mat4(1.0f), b.position);
    }

    // remote smoothing
    for (auto& b : m_Boids) {
        if (b.isLocallyOwned || !b.hasReplicatedState) continue;

        const glm::vec3 delta = b.replicatedTargetPos - b.position;
        const float dist = glm::length(delta);
        if (dist > m_RemoteSnapDistance) {
            b.position = b.replicatedTargetPos;
            b.velocity = b.replicatedTargetVel;
        }
        else {
            const float a = 1.0f - std::exp(-m_RemoteInterpRate * dt);
            b.position = glm::mix(b.position, b.replicatedTargetPos, a);
            b.velocity = glm::mix(b.velocity, b.replicatedTargetVel, a);
        }

        b.model = glm::translate(glm::mat4(1.0f), b.position);
    }

    const auto t1 = std::chrono::steady_clock::now();
    m_LastUpdateMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

glm::vec4 FlockingScenario::BoidTint(const Boid& b) const
{
    if (!m_ShowOwnerTint) {
        const float s = glm::clamp(glm::length(b.velocity) / std::max(0.1f, m_Settings.maxSpeed), 0.0f, 1.0f);
        return glm::vec4(0.2f + 0.8f * s, 0.9f - 0.5f * s, 1.0f - 0.7f * s, 1.0f);
    }

    switch (b.owner) {
    case SimRuntime::OwnerType::One: return { 1.0f, 0.3f, 0.3f, 1.0f };
    case SimRuntime::OwnerType::Two: return { 0.3f, 1.0f, 0.3f, 1.0f };
    case SimRuntime::OwnerType::Three: return { 0.3f, 0.5f, 1.0f, 1.0f };
    case SimRuntime::OwnerType::Four: return { 1.0f, 1.0f, 0.3f, 1.0f };
    default: return { 1,1,1,1 };
    }
}

void FlockingScenario::OnRender(VkCommandBuffer commandBuffer)
{
    std::lock_guard<std::mutex> lock(m_BoidsMutex);

    for (const auto& b : m_Boids) {
        PushConstants push{};
        push.model = b.model;

        const glm::vec4 tint = BoidTint(b);
        push.checkerColorA = tint;
        push.checkerColorB = glm::mix(tint, glm::vec4(0.08f, 0.08f, 0.08f, 1.0f), 0.65f);
        push.checkerParams = glm::vec4(4.0f, 0, 0, 0);

        vkCmdPushConstants(commandBuffer, m_App->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &push);

        VkBuffer vertexBuffers[] = { m_BoidBuffers.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_BoidBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, m_BoidBuffers.indexCount, 1, 0, 0, 0);
    }
}

void FlockingScenario::SendOwnedBoidStates()
{
    if (!m_NetworkingActive) return;

    for (const auto& b : m_Boids) {
        if (!b.isLocallyOwned) continue;

        SimStatePacket p{};
        p.objectId = b.id;
        p.owner = static_cast<uint8_t>(b.owner);
        p.pos[0] = b.position.x; p.pos[1] = b.position.y; p.pos[2] = b.position.z;
        p.vel[0] = b.velocity.x; p.vel[1] = b.velocity.y; p.vel[2] = b.velocity.z;
        p.tick = m_NetTick;
        m_Network.SendState(p);

        m_TxPackets.fetch_add(1);
    }

    ++m_NetTick;
}

void FlockingScenario::ReceiveRemoteBoidStates(float dt)
{
    if (!m_NetworkingActive) return;

    auto applyPacket = [this](const SimStatePacket& p) {
        Boid* b = FindBoidById(p.objectId);
        if (!b || b->isLocallyOwned) return;
        b->replicatedTargetPos = { p.pos[0], p.pos[1], p.pos[2] };
        b->replicatedTargetVel = { p.vel[0], p.vel[1], p.vel[2] };
        b->hasReplicatedState = true;
        m_RxPackets.fetch_add(1);
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
            if (lossDist(m_NetRng) < lossPct) continue; // Drop packet

            const float delayedMs = std::max(0.0f, baseMs + jitterDist(m_NetRng));
            m_DelayedIncomingStates.push_back({ p, delayedMs * 0.001f });
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

void FlockingScenario::StartNetworkWorker()
{
    if (m_RunNetworkThread.exchange(true)) return;
    m_NetworkThread = std::thread([this]() { NetworkWorkerMain(); });

#ifdef _WIN32
    if (const DWORD_PTR netMask = GetNetworkingAffinityMask(); netMask != 0) {
        SetThreadAffinityMask(
            reinterpret_cast<HANDLE>(m_NetworkThread.native_handle()),
            netMask
        );

        SetThreadDescription(
            reinterpret_cast<HANDLE>(m_NetworkThread.native_handle()),
            L"NetworkWorker_Flocking"
        );
    }
#endif
}

void FlockingScenario::StopNetworkWorker()
{
    if (!m_RunNetworkThread.exchange(false)) return;
    if (m_NetworkThread.joinable()) m_NetworkThread.join();
}

void FlockingScenario::NetworkWorkerMain()
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
            std::lock_guard<std::mutex> lock(m_BoidsMutex);
            ReceiveRemoteBoidStates(dt);
            SendOwnedBoidStates();
        }

        if (dt > 0.0001f) {
            m_NetworkMeasuredHz.store(1.0f / dt);
        }

        std::this_thread::sleep_for(step);
    }
}

void FlockingScenario::OnImGui()
{
    ImGui::Begin("Flocking (Networked)");

    std::lock_guard<std::mutex> lock(m_BoidsMutex);

    const char* peerNames[] = { "ONE", "TWO", "THREE", "FOUR" };
    int localPeer = static_cast<int>(m_LocalPeerOwner);
    if (ImGui::Combo("Local Peer", &localPeer, peerNames, IM_ARRAYSIZE(peerNames))) {
        m_LocalPeerOwner = static_cast<SimRuntime::OwnerType>(localPeer);
        RefreshOwnershipFlags();
    }

    int flockOwner = static_cast<int>(m_FlockOwner);
    if (ImGui::Combo("Flock Owner", &flockOwner, peerNames, IM_ARRAYSIZE(peerNames))) {
        m_FlockOwner = static_cast<SimRuntime::OwnerType>(flockOwner);
        RefreshOwnershipFlags();
    }

    ImGui::Text("Boids: %d", static_cast<int>(m_Boids.size()));
    ImGui::Text("Local-owned: %d", m_LocalOwnedCount);
    ImGui::Text("Remote: %d", m_RemoteCount);

    ImGui::Separator();
    ImGui::Text("Flocking Controls");
    ImGui::SliderFloat("Neighbor Radius", &m_Settings.neighborRadius, 0.2f, 12.0f, "%.2f");
    ImGui::SliderFloat("Separation Radius", &m_Settings.separationRadius, 0.1f, 6.0f, "%.2f");
    ImGui::SliderFloat("Avoidance Radius", &m_Settings.avoidanceRadius, 0.1f, 6.0f, "%.2f");
    ImGui::SliderFloat("Max Speed", &m_Settings.maxSpeed, 0.2f, 20.0f, "%.2f");
    ImGui::SliderFloat("Max Force", &m_Settings.maxForce, 0.2f, 30.0f, "%.2f");
    ImGui::SliderFloat("Weight Cohesion", &m_Settings.weightCohesion, 0.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Weight Alignment", &m_Settings.weightAlignment, 0.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Weight Separation", &m_Settings.weightSeparation, 0.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Weight Avoidance", &m_Settings.weightAvoidance, 0.0f, 5.0f, "%.2f");
    ImGui::Checkbox("Owner Tint", &m_ShowOwnerTint);

    const char* searchModes[] = { "Brute Force", "Uniform Grid", "Octree (WIP)" };
    int searchMode = static_cast<int>(m_SearchMode);
    if (ImGui::Combo("Neighbor Search", &searchMode, searchModes, IM_ARRAYSIZE(searchModes))) {
        m_SearchMode = static_cast<NeighborSearchMode>(searchMode);
    }

    ImGui::Text("Update CPU: %.3f ms", m_LastUpdateMs);
    if (m_SearchMode == NeighborSearchMode::UniformGrid) {
        ImGui::Text("Grid Build CPU: %.3f ms", m_LastGridBuildMs);
        ImGui::Text("Grid Cells: %u", m_LastGridCellCount);
        ImGui::Text("Grid Entries: %u", m_LastGridEntryCount);
    }
    else if (m_SearchMode == NeighborSearchMode::Octree) {
        ImGui::Text("Octree Build CPU: %.3f ms", m_LastOctreeBuildMs);
        ImGui::Text("Octree Nodes: %u", m_LastOctreeNodeCount);
    }

    if (ImGui::Button("Capture Sample (Current Mode)")) {
        const int modeIndex = static_cast<int>(m_SearchMode);
        m_ModeSamples[modeIndex].valid = true;
        m_ModeSamples[modeIndex].updateMs = m_LastUpdateMs;
        m_ModeSamples[modeIndex].buildMs = (m_SearchMode == NeighborSearchMode::UniformGrid) ? m_LastGridBuildMs
            : (m_SearchMode == NeighborSearchMode::Octree) ? m_LastOctreeBuildMs : 0.0f;
        m_ModeSamples[modeIndex].memoryBytes = EstimateModeMemoryBytes(m_SearchMode);
    }

    ImGui::Separator();
    ImGui::Text("Segmentation Comparison (captured)");
    const char* modeNames[] = { "BruteForce", "UniformGrid", "Octree" };
    for (int mi = 0; mi < 3; ++mi) {
        const auto& s = m_ModeSamples[mi];
        if (!s.valid) {
            ImGui::Text("%s: (no sample)", modeNames[mi]);
        }
        else {
            ImGui::Text("%s | Update: %.3f ms | Build: %.3f ms | Mem: %u bytes",
                modeNames[mi], s.updateMs, s.buildMs, s.memoryBytes);
        }
    }

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
    ImGui::Text("Network CPU Index: %d (expected 1..2)", m_LastNetworkCpu.load());

    ImGui::End();
}

FlockingScenario::GridKey FlockingScenario::ToGridKey(const glm::vec3& p) const
{
    const float cell = std::max(0.1f, m_Settings.neighborRadius);
    return {
        static_cast<int>(std::floor(p.x / cell)),
        static_cast<int>(std::floor(p.y / cell)),
        static_cast<int>(std::floor(p.z / cell))
    };
}

void FlockingScenario::BuildUniformGrid()
{
    m_UniformGrid.clear();

    for (size_t i = 0; i < m_Boids.size(); ++i) {
        const GridKey key = ToGridKey(m_Boids[i].position);
        m_UniformGrid[key].push_back(i);
    }

    m_LastGridCellCount = static_cast<uint32_t>(m_UniformGrid.size());
    m_LastGridEntryCount = static_cast<uint32_t>(m_Boids.size());
}

void FlockingScenario::CollectUniformGridCandidates(size_t boidIndex, std::vector<size_t>& outCandidates) const
{
    outCandidates.clear();
    const GridKey center = ToGridKey(m_Boids[boidIndex].position);

    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                GridKey key{ center.x + dx, center.y + dy, center.z + dz };
                auto it = m_UniformGrid.find(key);
                if (it == m_UniformGrid.end()) continue;
                outCandidates.insert(outCandidates.end(), it->second.begin(), it->second.end());
            }
        }
    }
}

FlockingScenario::Aabb FlockingScenario::ComputeFlockBounds(float padding) const
{
    Aabb box{};
    if (m_Boids.empty()) return box;

    box.min = m_Boids[0].position;
    box.max = m_Boids[0].position;

    for (const auto& b : m_Boids) {
        box.min = glm::min(box.min, b.position);
        box.max = glm::max(box.max, b.position);
    }

    box.min -= glm::vec3(padding);
    box.max += glm::vec3(padding);
    return box;
}

bool FlockingScenario::AabbContainsPoint(const Aabb& box, const glm::vec3& p)
{
    return p.x >= box.min.x && p.x <= box.max.x &&
           p.y >= box.min.y && p.y <= box.max.y &&
           p.z >= box.min.z && p.z <= box.max.z;
}

bool FlockingScenario::AabbIntersectsSphere(const Aabb& box, const glm::vec3& c, float r)
{
    const glm::vec3 q = glm::clamp(c, box.min, box.max);
    const glm::vec3 d = q - c;
    return glm::dot(d, d) <= (r * r);
}

std::unique_ptr<FlockingScenario::OctreeNode> FlockingScenario::BuildOctreeNode(const Aabb& box, const std::vector<size_t>& indices, int depth)
{
    auto node = std::make_unique<OctreeNode>();
    node->bounds = box;
    ++m_LastOctreeNodeCount;

    if (depth >= m_OctreeMaxDepth || static_cast<int>(indices.size()) <= m_OctreeLeafCapacity) {
        node->indices = indices;
        return node;
    }

    const glm::vec3 c = (box.min + box.max) * 0.5f;
    std::array<std::vector<size_t>, 8> buckets{};

    for (size_t idx : indices) {
        const glm::vec3& p = m_Boids[idx].position;
        int oct = 0;
        if (p.x >= c.x) oct |= 1;
        if (p.y >= c.y) oct |= 2;
        if (p.z >= c.z) oct |= 4;
        buckets[oct].push_back(idx);
    }

    for (int i = 0; i < 8; ++i) {
        if (buckets[i].empty()) continue;

        Aabb child{};
        child.min.x = (i & 1) ? c.x : box.min.x;
        child.max.x = (i & 1) ? box.max.x : c.x;
        child.min.y = (i & 2) ? c.y : box.min.y;
        child.max.y = (i & 2) ? box.max.y : c.y;
        child.min.z = (i & 4) ? c.z : box.min.z;
        child.max.z = (i & 4) ? box.max.z : c.z;

        node->children[i] = BuildOctreeNode(child, buckets[i], depth + 1);
    }

    return node;
}

void FlockingScenario::BuildOctree()
{
    m_OctreeRoot.reset();
    m_LastOctreeNodeCount = 0;

    if (m_Boids.empty()) return;

    std::vector<size_t> all;
    all.reserve(m_Boids.size());
    for (size_t i = 0; i < m_Boids.size(); ++i) all.push_back(i);

    const Aabb rootBounds = ComputeFlockBounds(std::max(0.5f, m_Settings.neighborRadius));
    m_OctreeRoot = BuildOctreeNode(rootBounds, all, 0);
}

void FlockingScenario::CollectOctreeCandidates(const OctreeNode* node, const glm::vec3& p, float radius, std::vector<size_t>& out) const
{
    if (!node) return;
    if (!AabbIntersectsSphere(node->bounds, p, radius)) return;

    bool hasChildren = false;
    for (const auto& ch : node->children) {
        if (ch) { hasChildren = true; break; }
    }

    if (!hasChildren) {
        out.insert(out.end(), node->indices.begin(), node->indices.end());
        return;
    }

    for (const auto& ch : node->children) {
        if (ch) CollectOctreeCandidates(ch.get(), p, radius, out);
    }
}

uint32_t FlockingScenario::EstimateModeMemoryBytes(NeighborSearchMode mode) const
{
    switch (mode) {
    case NeighborSearchMode::BruteForce:
        return 0u;
    case NeighborSearchMode::UniformGrid:
    {
        // rough estimate
        uint64_t bytes = 0;
        bytes += static_cast<uint64_t>(m_UniformGrid.size()) * (sizeof(GridKey) + sizeof(std::vector<size_t>));
        bytes += static_cast<uint64_t>(m_Boids.size()) * sizeof(size_t);
        return static_cast<uint32_t>(bytes);
    }
    case NeighborSearchMode::Octree:
    {
        uint64_t bytes = 0;
        bytes += static_cast<uint64_t>(m_LastOctreeNodeCount) * sizeof(OctreeNode);
        bytes += static_cast<uint64_t>(m_Boids.size()) * sizeof(size_t);
        return static_cast<uint32_t>(bytes);
    }
    default:
        return 0u;
    }
}