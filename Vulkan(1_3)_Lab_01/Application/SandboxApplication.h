#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../Renderer/VulkanCore.h"
#include "../Scenarios/Scenario.h"
#include "../UI/ImGuiLayer.h"

#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <array>

// --- Configuration ---
constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

enum class CameraView { Top, Front, Side, Perspective };

// --- Vertex Data ---
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

class SandboxApplication {
public:
    void Run();
    ~SandboxApplication();

    // --- Scenario Management ---
    template<typename T>
    void RegisterScenario(const std::string& name) {
        m_ScenarioFactories.push_back({ name, [this]() {
            return std::make_unique<T>(this);
        }});
    }
    void ChangeScenario(int index);

    // --- Simulation Controls ---
    void Play() { m_IsPaused = false; }
    void Pause() { m_IsPaused = true; }
    void TogglePause() { m_IsPaused = !m_IsPaused; }
    void Stop();
    bool IsPaused() const { return m_IsPaused; }
    void SetTimeStep(float dt) { m_TimeStep = dt; }
    float GetTimeStep() const { return m_TimeStep; }
    void SetSimulationSpeed(float speed) { m_SimulationSpeed = speed; }
    float GetSimulationSpeed() const { return m_SimulationSpeed; }

    // --- Camera ---
    void SetCameraView(CameraView view) { m_CameraView = view; }
    CameraView GetCameraView() const { return m_CameraView; }

    // --- Vulkan Access for Scenarios ---
    VkDevice GetDevice() const { return m_Device; }
    VkCommandPool GetCommandPool() const { return m_CommandPool; }

private:
    // --- Initialization ---
    void initWindow();
    void initVulkan();
    void initImGui();
    void initScenarios();
    void mainLoop();
    void cleanup();

    // --- Vulkan Setup ---
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();

    // --- Rendering ---
    void drawFrame();
    void recreateSwapChain();
    void cleanupSwapChain();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateUniformBuffer(uint32_t currentImage);

    // --- Helpers ---
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    std::vector<const char*> getRequiredExtensions();
    bool checkValidationLayerSupport();
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                      VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                      VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // --- Callbacks ---
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

private:
    // --- Window ---
    GLFWwindow* m_Window = nullptr;
    bool m_FramebufferResized = false;

    // --- Vulkan Core ---
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    uint32_t m_GraphicsQueueFamily = 0;

    // --- Swapchain ---
    VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_SwapChainImages;
    VkFormat m_SwapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_SwapChainExtent{0, 0};
    std::vector<VkImageView> m_SwapChainImageViews;

    // --- Pipeline ---
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;

    // --- Buffers ---
    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
    std::vector<VkBuffer> m_UniformBuffers;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;
    std::vector<void*> m_UniformBuffersMapped;

    // --- Descriptors ---
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    // --- Synchronization ---
    std::vector<VkCommandBuffer> m_CommandBuffers;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    uint32_t m_CurrentFrame = 0;

    // --- Scenario System ---
    std::unique_ptr<Scenario> m_CurrentScenario;
    std::vector<std::pair<std::string, std::function<std::unique_ptr<Scenario>()>>> m_ScenarioFactories;
    int m_CurrentScenarioIndex = 0;

    // --- UI ---
    std::unique_ptr<ImGuiLayer> m_UILayer;

    // --- Simulation State ---
    bool m_IsPaused = true;
    float m_TimeStep = 1.0f / 60.0f;
    float m_SimulationSpeed = 1.0f;
    float m_AccumulatedTime = 0.0f;

    // --- Camera ---
    CameraView m_CameraView = CameraView::Perspective;
};