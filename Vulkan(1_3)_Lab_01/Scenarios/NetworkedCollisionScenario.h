#pragma once

#include "Scenario.h"
#include "../Application/SandboxApplication.h"
#include "../Networking/NetworkPeer.h"
#include "../Renderer/MeshGenerator.h"
#include "../Scene/SceneRuntime.h"
#include "../SimulationLibrary/Collider.h"
#include "../SimulationLibrary/PhysicsObject.h"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <atomic>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

class NetworkedCollisionScenario : public Scenario {
private:
    enum class DemoPreset : int {
        BounceCorner_SpherePlaneWall = 0,
        SphereVsSphere_HeadOn = 1,
        SphereVsCuboid_RotatedOBB = 2,
        CuboidVsCuboid_AxisAligned = 3,
        CuboidVsPlane_Tilted = 4,

        // NEW: your requested "many objects in (almost) enclosed arena" + spawners
        Arena_ManyObjects_Spawners = 5
    };

    struct SphereInstance {
        uint32_t id = 0;
        SimRuntime::OwnerType owner = SimRuntime::OwnerType::One;
        bool isLocallyOwned = true;

        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::vec3 color{ 1.0f, 0.3f, 0.3f };

        bool hasReplicatedState = false;
        glm::vec3 replicatedTargetPos{ 0.0f };
        glm::vec3 replicatedTargetVel{ 0.0f };
    };

    struct PlaneInstance {
        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::quat orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec2 size{ 10.0f, 10.0f };
        glm::vec3 color{ 0.4f, 0.4f, 0.4f };
    };

    struct BoxInstance {
        uint32_t id = 0;
        SimRuntime::OwnerType owner = SimRuntime::OwnerType::One;
        bool isLocallyOwned = true;

        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::vec3 halfExtents{ 0.6f, 0.6f, 0.6f };
        glm::vec3 color{ 0.3f, 0.8f, 1.0f };

        bool hasReplicatedState = false;
        glm::vec3 replicatedTargetPos{ 0.0f };
        glm::vec3 replicatedTargetVel{ 0.0f };
    };

private:
    // Scene content
    std::vector<SphereInstance> m_Spheres;
    std::vector<PlaneInstance> m_Planes;
    std::vector<BoxInstance> m_Boxes;

    uint32_t m_NextObjectId = 1;
    DemoPreset m_Preset = DemoPreset::BounceCorner_SpherePlaneWall;

    // Sim settings
    float m_Gravity = -9.81f;
    bool m_UseBounce = true;
    float m_BounceRestitution = 0.85f;

    // NEW: keep objects moving
    float m_MinDynamicSpeed = 1.25f;

    // Ownership
    SimRuntime::OwnerType m_LocalPeerOwner = SimRuntime::OwnerType::One;
    SimRuntime::OwnerType m_SimOwner = SimRuntime::OwnerType::One; // who simulates the dynamic bodies in the preset
    bool m_ShowOwnerTint = true;

    // Remote smoothing
    float m_RemoteInterpRate = 12.0f;
    float m_RemoteSnapDistance = 1.5f;

    // Networking
    NetworkPeer m_Network;
    bool m_NetworkingActive = false;
    int m_LocalPort = 27000;
    int m_RemotePort = 27001;
    char m_RemoteIp[64] = "127.0.0.1";
    uint32_t m_NetTick = 0;

    std::thread m_NetworkThread;
    std::atomic<bool> m_RunNetworkThread{ false };
    float m_NetworkTargetHz = 30.0f;
    std::atomic<float> m_NetworkMeasuredHz{ 0.0f };

    std::atomic<uint64_t> m_TxPackets{ 0 };
    std::atomic<uint64_t> m_RxPackets{ 0 };
    std::atomic<int> m_LastNetworkCpu{ -1 };

    std::atomic<int> m_PendingPreset{ -1 };
    std::atomic<bool> m_PendingReset{ false };

    std::mutex m_Mutex;

    // NEW: UI spawner controls
    float m_SpawnSphereRadius = 0.35f;
    glm::vec3 m_SpawnBoxHalfExtents{ 0.35f, 0.35f, 0.35f };
    float m_SpawnMass = 2.0f;
    float m_SpawnSpeed = 8.0f;
    float m_SpawnSpreadDeg = 10.0f;

    // Deterministic random (shared across peers for preset build)
    std::mt19937 m_Rng{ 1337u };

private:
    void ClearScene();
    void SetupPreset(DemoPreset preset);

    void UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position);

    void AddPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec2& size, const glm::vec3& color);
    void AddSphere(const glm::vec3& position, const glm::vec3& velocity, float radius, float mass, const glm::vec3& color, SimRuntime::OwnerType owner);
    void AddBox(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& halfExtents, const glm::vec3& velocity, float mass, const glm::vec3& color, SimRuntime::OwnerType owner);

    // NEW: static “capsule/cylinder” obstacles (rendered as such, collision approximated by BoxCollider)
    void AddStaticCylinderObstacle(const glm::vec3& position, float radius, float height, const glm::quat& orientation, const glm::vec3& color);
    void AddStaticCapsuleObstacle(const glm::vec3& position, float radius, float height, const glm::quat& orientation, const glm::vec3& color);

    void ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane);
    void ResolveSphereSphere(SphereInstance& a, SphereInstance& b);
    void ResolveSphereBox(SphereInstance& sphere, BoxInstance& box);
    void ResolveBoxPlane(BoxInstance& box, const PlaneCollider& plane);
    void ResolveBoxBox(BoxInstance& a, BoxInstance& b);

    void ApplyRemoteSmoothing(float dt);

    void SendOwnedStates_NoLock();
    void ReceiveRemoteStates_NoLock();
    void ReceiveRemoteCommands_NoLock();

    // NEW: spawn replication
    void ReceiveRemoteSpawns_NoLock();
    void SendSpawn_NoLock(const SimSpawnPacket& p);

    void StartNetworkWorker();
    void StopNetworkWorker();
    void NetworkWorkerMain();

    void SendSetPreset(DemoPreset preset);
    void SendReset();

    // NEW: min-speed clamp
    void EnforceMinimumSpeed_NoLock();

    // NEW: UI spawners
    void SpawnSphereFromUI_NoLock();
    void SpawnBoxFromUI_NoLock();

public:
    explicit NetworkedCollisionScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnImGui() override;
    void OnUnload() override;

    std::string GetName() const override { return "Collision (Networked)"; }

    void GetSelectionItems(std::vector<SceneSelectionItem>& out) override;
};