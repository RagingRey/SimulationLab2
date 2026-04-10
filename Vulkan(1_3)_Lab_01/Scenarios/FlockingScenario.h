#pragma once

#include "Scenario.h"
#include "../Application/SandboxApplication.h"
#include "../Renderer/MeshGenerator.h"
#include "../Scene/SceneRuntime.h"
#include "../Networking/NetworkPeer.h"

#include <glm/glm.hpp>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <unordered_map>
#include <cstddef>
#include <memory>
#include <array>

class FlockingScenario : public Scenario {
private:
    struct Boid {
        uint32_t id = 0;
        SimRuntime::OwnerType owner = SimRuntime::OwnerType::One;
        bool isLocallyOwned = true;

        glm::vec3 position{ 0.0f };
        glm::vec3 velocity{ 0.0f };
        glm::mat4 model{ 1.0f };

        glm::vec3 initialPosition{ 0.0f };
        glm::vec3 initialVelocity{ 0.0f };

        bool hasReplicatedState = false;
        glm::vec3 replicatedTargetPos{ 0.0f };
        glm::vec3 replicatedTargetVel{ 0.0f };
    };

    enum class NeighborSearchMode {
        BruteForce = 0,
        UniformGrid = 1,
        Octree = 2
    };

    struct GridKey {
        int x = 0;
        int y = 0;
        int z = 0;

        bool operator==(const GridKey& rhs) const {
            return x == rhs.x && y == rhs.y && z == rhs.z;
        }
    };

    struct GridKeyHasher {
        size_t operator()(const GridKey& k) const noexcept {
            size_t h = static_cast<size_t>(k.x) * 73856093u;
            h ^= static_cast<size_t>(k.y) * 19349663u;
            h ^= static_cast<size_t>(k.z) * 83492791u;
            return h;
        }
    };

    struct Aabb {
        glm::vec3 min{ 0.0f };
        glm::vec3 max{ 0.0f };
    };

    struct OctreeNode {
        Aabb bounds{};
        std::vector<size_t> indices;
        std::array<std::unique_ptr<OctreeNode>, 8> children{};
    };

    struct ModeSample {
        bool valid = false;
        float updateMs = 0.0f;
        float buildMs = 0.0f;
        uint32_t memoryBytes = 0;
    };

    std::vector<Boid> m_Boids;
    Mesh m_BoidMesh;
    SandboxApplication::MeshBuffers m_BoidBuffers{};

    SimRuntime::FlockingSettingsDef m_Settings{};
    SimRuntime::OwnerType m_FlockOwner = SimRuntime::OwnerType::One;
    SimRuntime::OwnerType m_LocalPeerOwner = SimRuntime::OwnerType::One;
    uint32_t m_NextBoidId = 1;

    NetworkPeer m_Network;
    bool m_NetworkingActive = false;
    int m_LocalPort = 26000;
    int m_RemotePort = 26001;
    char m_RemoteIp[64] = "127.0.0.1";
    uint32_t m_NetTick = 0;

    std::thread m_NetworkThread;
    std::atomic<bool> m_RunNetworkThread{ false };
    float m_NetworkTargetHz = 30.0f;
    std::atomic<float> m_NetworkMeasuredHz{ 0.0f };

    std::mutex m_BoidsMutex;
    std::mt19937 m_Rng{ std::random_device{}() };

    float m_RemoteInterpRate = 12.0f;
    float m_RemoteSnapDistance = 1.5f;

    bool m_ShowOwnerTint = true;
    float m_BoidRadius = 0.2f;
    int m_LocalOwnedCount = 0;
    int m_RemoteCount = 0;

    std::atomic<uint64_t> m_TxPackets{ 0 };
    std::atomic<uint64_t> m_RxPackets{ 0 };

    std::atomic<int> m_LastNetworkCpu{ -1 };

    NeighborSearchMode m_SearchMode = NeighborSearchMode::BruteForce;
    std::unordered_map<GridKey, std::vector<size_t>, GridKeyHasher> m_UniformGrid;
    float m_LastGridBuildMs = 0.0f;
    float m_LastUpdateMs = 0.0f;
    uint32_t m_LastGridCellCount = 0;
    uint32_t m_LastGridEntryCount = 0;

    std::unique_ptr<OctreeNode> m_OctreeRoot;
    float m_LastOctreeBuildMs = 0.0f;
    uint32_t m_LastOctreeNodeCount = 0;
    int m_OctreeMaxDepth = 6;
    int m_OctreeLeafCapacity = 12;

    std::array<ModeSample, 3> m_ModeSamples{};

    void BuildFromSceneOrFallback();
    void BuildBoids(uint32_t count, const glm::vec3& center, const glm::vec3& extents);
    void ResetBoids();
    void ResetBoids_NoLock();
    void RefreshOwnershipFlags();
    Boid* FindBoidById(uint32_t id);

    GridKey ToGridKey(const glm::vec3& p) const;
    void BuildUniformGrid();
    void CollectUniformGridCandidates(size_t boidIndex, std::vector<size_t>& outCandidates) const;

    glm::vec3 ComputeSteering(size_t boidIndex) const;
    glm::vec4 BoidTint(const Boid& b) const;

    void SendOwnedBoidStates();
    void ReceiveRemoteBoidStates();

    void StartNetworkWorker();
    void StopNetworkWorker();
    void NetworkWorkerMain();

    Aabb ComputeFlockBounds(float padding) const;
    static bool AabbContainsPoint(const Aabb& box, const glm::vec3& p);
    static bool AabbIntersectsSphere(const Aabb& box, const glm::vec3& c, float r);
    std::unique_ptr<OctreeNode> BuildOctreeNode(const Aabb& box, const std::vector<size_t>& indices, int depth);
    void BuildOctree();
    void CollectOctreeCandidates(const OctreeNode* node, const glm::vec3& p, float radius, std::vector<size_t>& out) const;

    uint32_t EstimateModeMemoryBytes(NeighborSearchMode mode) const;

public:
    explicit FlockingScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnUnload() override;
    void OnImGui() override;
    std::string GetName() const override { return "Flocking (Networked)"; }
};