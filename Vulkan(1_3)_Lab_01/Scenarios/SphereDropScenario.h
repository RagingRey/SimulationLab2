#pragma once
#include "Scenario.h"
#include "../Renderer/MeshGenerator.h"
#include "../Application/SandboxApplication.h"
#include <glm/glm.hpp>

class SphereDropScenario : public Scenario {
private:
    glm::vec3 m_SpherePosition = {0.0f, 5.0f, 0.0f};
    glm::vec3 m_Velocity = {0.0f, 0.0f, 0.0f};
    float m_Radius = 0.5f;
    float m_Gravity = -9.81f;
    float m_GroundY = 0.0f;

    Mesh m_SphereMesh;
    Mesh m_GroundMesh;
    SandboxApplication::MeshBuffers m_SphereBuffers{};
    SandboxApplication::MeshBuffers m_GroundBuffers{};

public:
    explicit SphereDropScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnImGui() override;
    void OnUnload() override;
    std::string GetName() const override { return "Sphere Drop Test"; }
};