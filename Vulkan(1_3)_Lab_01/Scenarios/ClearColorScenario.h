#pragma once
#include "Scenario.h"
#include <glm/glm.hpp>

class ClearColorScenario : public Scenario {
private:
    glm::vec4 m_ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };

public:
    explicit ClearColorScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override {}
    void OnUpdate(float) override {}
    void OnRender(VkCommandBuffer) override {}
    void OnUnload() override {}

    void OnImGui() override;
    void ImGuiMainMenu() override;

    glm::vec4 GetClearColor() const override { return m_ClearColor; }
    std::string GetName() const override { return "Clear Color"; }
};