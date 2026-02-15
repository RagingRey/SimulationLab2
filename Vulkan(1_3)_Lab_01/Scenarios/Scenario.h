#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <glm/glm.hpp>

// Forward declaration
class SandboxApplication;

class Scenario {
protected:
    SandboxApplication* m_App;

public:
    explicit Scenario(SandboxApplication* app) : m_App(app) {}
    virtual ~Scenario() = default;

    virtual void OnLoad() = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnRender(VkCommandBuffer commandBuffer) = 0;
    virtual void OnUnload() = 0;
    virtual void OnImGui() {}  // Optional per-scenario UI
    virtual void ImGuiMainMenu() {} // Optional per-scenario main menu items
    virtual glm::vec4 GetClearColor() const { return { 0.1f, 0.1f, 0.1f, 1.0f }; }
    virtual std::string GetName() const = 0;
};