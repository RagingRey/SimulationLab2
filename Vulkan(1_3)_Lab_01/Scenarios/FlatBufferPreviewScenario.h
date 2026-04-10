#pragma once

#include "Scenario.h"
#include "../Renderer/MeshGenerator.h"
#include "../Application/SandboxApplication.h"
#include "../Scene/SceneRuntime.h"
#include "../Networking/NetworkPeer.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <random>

class FlatBufferPreviewScenario : public Scenario {
private:
    enum class DisplayMode {
        Owner = 0,
        Material = 1
    };

    struct RenderItem {
        std::string name;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::mat4 model{ 1.0f };
        std::string material;
        SimRuntime::BehaviourType behaviourType = SimRuntime::BehaviourType::Static;
        SimRuntime::OwnerType owner = SimRuntime::OwnerType::One;
        uint32_t objectId = 0;

        bool isPlane = false;
        bool isLocallyOwned = true;

        // animated runtime
        SimRuntime::Transform baseTransform{};
        std::vector<SimRuntime::Waypoint> waypoints;
        float totalDuration = 0.0f;
        SimRuntime::EasingType easing = SimRuntime::EasingType::Linear;
        SimRuntime::PathMode pathMode = SimRuntime::PathMode::Stop;
        float animTime = 0.0f;
        bool reverse = false;

        // simulated runtime (NEW)
        bool isSimulated = false;
        glm::vec3 linearVelocity{ 0.0f };
        float inverseMass = 0.0f;
        float restitution = 0.45f;
        float boundRadius = 0.5f;

        glm::mat4 initialModel{ 1.0f };
        SimRuntime::Transform initialBaseTransform{};
        glm::vec3 initialLinearVelocity{ 0.0f };

        // remote replication smoothing
        bool hasReplicatedState = false;
        glm::vec3 replicatedTargetPos{ 0.0f };
        glm::vec3 replicatedTargetVel{ 0.0f };

        bool spawnedBySpawner = false;
    };

    struct SpawnerRuntime {
        SimRuntime::SpawnerDef def{};
        bool singleBurstDone = false;
        float elapsed = 0.0f;
        float repeatAccumulator = 0.0f;
        uint32_t spawnedCount = 0;
        uint32_t sequentialCursor = 0; // for SEQUENTIAL ownership rotation
    };

    std::vector<RenderItem> m_Items;
    std::vector<std::string> m_LocalWarnings;
    int m_UnsupportedCount = 0;
    bool m_UsingFallbackData = false;
    DisplayMode m_DisplayMode = DisplayMode::Owner;

    SimRuntime::OwnerType m_LocalPeerOwner = SimRuntime::OwnerType::One;
    int m_LocalOwnedSimulatedCount = 0;
    int m_RemoteSimulatedCount = 0;

    NetworkPeer m_Network;
    std::atomic<bool> m_NetworkingActive { false};
    int m_LocalPort = 25000;
    int m_RemotePort = 25001;
    char m_RemoteIp[64] = "127.0.0.1";
    uint32_t m_NetTick = 0;
    uint32_t m_NextObjectId = 1;

    std::thread m_NetworkThread;
    std::atomic<bool> m_RunNetworkThread{ false };
    float m_NetworkTargetHz = 30.0f;
    std::atomic<float> m_NetworkMeasuredHz{ 0.0f };
    std::mutex m_ItemsMutex;

    float m_RemoteInterpRate = 12.0f;      // higher = tighter follow
    float m_RemoteSnapDistance = 1.0f;     // snap if too far away

    std::atomic<bool> m_EnableNetEmulation{ false };
    std::atomic<float> m_EmuBaseLatencyMs{ 100.0f };
    std::atomic<float> m_EmuJitterMs{ 50.0f };
    std::atomic<float> m_EmuLossPercent{ 20.0f };

    std::atomic<bool> m_ResyncSnapshotRequested{ false };

    struct DelayedStatePacket {
        SimStatePacket packet{};
        float remainingDelaySec = 0.0f;
    };

    std::deque<DelayedStatePacket> m_DelayedIncomingStates;
    std::mt19937 m_NetRng{ std::random_device{}() };

    std::vector<SpawnerRuntime> m_RuntimeSpawners;
    std::mt19937 m_SpawnRng{ 1337u };
    bool m_EnableRuntimeSpawners = true;
    uint32_t m_TotalSpawnedBySpawners = 0;

    void Clear();
    void BuildFromLoadedScene();
    void BuildFallbackScene();
    static glm::mat4 BuildModelMatrix(const SimRuntime::Transform& t);
    static Mesh BuildCuboidMesh(const glm::vec3& size, const glm::vec3& color);

    glm::vec4 OwnerColor(const RenderItem& item) const;
    glm::vec4 MaterialColor(const RenderItem& item) const;
    void RefreshOwnershipFlagsAndStats();
    RenderItem* FindItemById(uint32_t id);

    void SendOwnedSimulatedStates();
    void ReceiveRemoteSimulatedStates(float dt);

    void SendGlobalCommand(NetCommandType command, float value = 0.0f);
    void ReceiveAndApplyRemoteCommands();

    void StartNetworkWorker();
    void StopNetworkWorker();
    void NetworkWorkerMain();

    void ResetRuntimeState();

    void InitRuntimeSpawnersFromScene();
    void ResetRuntimeSpawners();
    void UpdateRuntimeSpawners(float dt);
    SimRuntime::OwnerType ResolveSpawnerOwner(SpawnerRuntime& s);
    glm::vec3 RandomVec3InRange(const SimRuntime::Vec3RangeDef& r);
    SimRuntime::Transform BuildSpawnTransform(const SimRuntime::SpawnLocationDef& loc);

    bool IsSpawnerAuthority(const SpawnerRuntime& s) const;
    void SendSpawnPacketForItem(const RenderItem& item, SimRuntime::SpawnerShapeType shape, float radius, float height, const glm::vec3& size);
    void ReceiveRemoteSpawnPackets();

    void SendResyncSnapshot();

public:
    explicit FlatBufferPreviewScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnUnload() override;
    void OnImGui() override;
    std::string GetName() const override { return "FlatBuffer Preview"; }
};