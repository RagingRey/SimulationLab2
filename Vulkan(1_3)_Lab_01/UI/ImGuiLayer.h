#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

class SandboxApplication;

class ImGuiLayer {
private:
    VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    bool m_Initialized = false;

    std::vector<std::string> m_ScenarioNames;
    int m_SelectedScenarioIndex = 0;

public:
    void Init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice,
              VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue,
              VkFormat swapChainFormat);
    void Shutdown(VkDevice device);
    
    void BeginFrame();
    void EndFrame(VkCommandBuffer commandBuffer);
    
    void RenderControlPanel(SandboxApplication* app);
    
    void SetScenarioNames(const std::vector<std::string>& names) { m_ScenarioNames = names; }
    void SetSelectedScenario(int index) { m_SelectedScenarioIndex = index; }
};