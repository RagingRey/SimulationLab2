#pragma once
#include "Scenario.h"
#include "../Renderer/MeshGenerator.h"
#include "../Application/SandboxApplication.h"
#include "../SimulationLibrary/PhysicsObject.h"
#include "../SimulationLibrary/Collider.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

class SphereDropScenario : public Scenario {
private:
    struct SphereInstance {
        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::vec3 color{ 1.0f, 0.3f, 0.3f };
    };

    struct PlaneInstance {
        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::vec3 color{ 0.4f, 0.4f, 0.4f };
        glm::quat orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec2 size{ 10.0f, 10.0f };
    };

    std::vector<SphereInstance> m_Spheres;
    std::vector<PlaneInstance> m_Planes;
    int m_GroundPlaneIndex = -1;

    float m_Gravity = -9.81f;
    float m_GroundY = 0.0f;
    bool m_UseBounce = true;

    float m_PlaneFriction = 0.35f;
    float m_StopSpeed = 0.05f;

    std::string m_ConfigPath = "Configs/SphereDropScenario.cfg";

    void ClearScene();
    void RebuildSphereMesh(SphereInstance& sphere);

    void ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane);
    void ResolveSphereSphere(SphereInstance& a, SphereInstance& b);

    void AddSphere(const glm::vec3& position,
        const glm::vec3& velocity,
        float radius,
        const glm::vec3& color,
        float restitution,
        float mass);

    void AddPlane(const glm::vec3& point,
        const glm::vec3& normal,
        const glm::vec2& size,
        const glm::vec3& color,
        bool isGround);

    void UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position);

    bool LoadConfig(const std::string& path);
    void LoadDefaultConfig();

public:
    explicit SphereDropScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnImGui() override;
    void ImGuiMainMenu() override;
    void OnUnload() override;
    std::string GetName() const override { return "Sphere Drop Test"; }
};