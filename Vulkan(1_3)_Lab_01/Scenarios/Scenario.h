#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <glm/glm.hpp>
#include <functional>
#include <vector>

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

    // --- Selection / Editor API ---
    struct TransformProxy {
        glm::vec3 position{ 0.0f };
        glm::vec3 rotationDeg{ 0.0f }; // Euler angles in degrees
        glm::vec3 scale{ 1.0f };
    };

    struct SceneSelectionItem {
        uint32_t id = 0;
        std::string name;
        std::function<TransformProxy()> GetTransform;
        std::function<void(const TransformProxy&)> SetTransform;
        std::function<void()> Delete; // optional
    };

    // Scenario should fill the vector with items it can expose; default empty
    virtual void GetSelectionItems(std::vector<SceneSelectionItem>& out) { out.clear(); }
};