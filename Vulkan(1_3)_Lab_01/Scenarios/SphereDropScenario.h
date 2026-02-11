#pragma once
#include "Scenario.h"
#include <glm/glm.hpp>

class SphereDropScenario : public Scenario {
private:
    glm::vec3 m_SpherePosition = {0.0f, 5.0f, 0.0f};
    glm::vec3 m_Velocity = {0.0f, 0.0f, 0.0f};
    float m_Radius = 0.5f;
    float m_Gravity = -9.81f;
    float m_GroundY = 0.0f;

public:
    explicit SphereDropScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override {
        m_SpherePosition = {0.0f, 5.0f, 0.0f};
        m_Velocity = {0.0f, 0.0f, 0.0f};
    }

    void OnUpdate(float deltaTime) override {
        m_Velocity.y += m_Gravity * deltaTime;
        m_SpherePosition += m_Velocity * deltaTime;

        if (m_SpherePosition.y - m_Radius < m_GroundY) {
            m_SpherePosition.y = m_GroundY + m_Radius;
            m_Velocity.y = -m_Velocity.y * 0.8f;
        }
    }

    void OnRender(VkCommandBuffer commandBuffer) override {
        // Step 4: Primitive rendering
    }

    void OnImGui() override {
        // Scenario-specific UI
    }

    void OnUnload() override {}

    std::string GetName() const override { return "Sphere Drop Test"; }
};