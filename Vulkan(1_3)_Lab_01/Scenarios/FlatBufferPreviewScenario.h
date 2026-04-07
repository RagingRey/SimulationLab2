#pragma once

#include "Scenario.h"
#include "../Renderer/MeshGenerator.h"
#include "../Application/SandboxApplication.h"
#include "../Scene/SceneRuntime.h"
#include "../Networking/NetworkPeer.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

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
    bool m_NetworkingActive = false;
    int m_LocalPort = 25000;
    int m_RemotePort = 25001;
    char m_RemoteIp[64] = "127.0.0.1";
    uint32_t m_NetTick = 0;
    uint32_t m_NextObjectId = 1;

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
    void ReceiveRemoteSimulatedStates();

    void SendGlobalCommand(NetCommandType command, float value = 0.0f);
    void ReceiveAndApplyRemoteCommands();

public:
    explicit FlatBufferPreviewScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnUnload() override;
    void OnImGui() override;
    std::string GetName() const override { return "FlatBuffer Preview"; }
};