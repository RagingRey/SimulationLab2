#include "SandboxApplication.h"
#include "../UI/ImGuiLayer.h"
#include "../Scenarios/SphereDropScenario.h"
#include "../Scenarios/ClearColorScenario.h"
#include "../Scenarios/CollisionScenario.h"
#include "../Scenarios/OrientationScenario.h"
#include "../Scene/SceneLoaderFlatBuffer.h"
#include "../Scenarios/FlatBufferPreviewScenario.h"
#include "../Scenarios/FlockingScenario.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
#include <limits>
#include <filesystem>
#include <unordered_set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#ifdef _WIN32
namespace
{
    DWORD_PTR GetVisualizationAffinityMask()
    {
        // Core 1 (spec wording) => CPU bit 0
        const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (count == 0) return 0;
        return (static_cast<DWORD_PTR>(1) << 0);
    }

    DWORD_PTR GetSimulationAffinityMask()
    {
        // Core 4+ => CPU bits 3..N
        const DWORD count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        if (count == 0) return 0;

        DWORD_PTR mask = 0;
        const DWORD maxBits = static_cast<DWORD>(sizeof(DWORD_PTR) * 8);
        const DWORD usable = (count < maxBits) ? count : maxBits;

        if (usable > 3) {
            for (DWORD i = 3; i < usable; ++i) {
                mask |= (static_cast<DWORD_PTR>(1) << i);
            }
        }
        else {
            // fallback on low-core systems
            mask = (static_cast<DWORD_PTR>(1) << (usable - 1));
        }

        return mask;
    }

    std::vector<std::string> CollectCandidateSceneFiles()
    {
        namespace fs = std::filesystem;

        std::vector<std::string> out;
        std::unordered_set<std::string> seen;

        auto addPath = [&](const fs::path& p) {
            std::error_code ec;
            if (!fs::exists(p, ec) || ec) return;
            if (!fs::is_regular_file(p, ec) || ec) return;
            if (p.extension() != ".bin") return;

            const std::string key = fs::absolute(p, ec).lexically_normal().string();
            if (ec) return;

            if (seen.insert(key).second) {
                out.push_back(key);
            }
            };

        // Explicit priority candidates first
        addPath("Scenes/scene.bin");
        addPath("../Scenes/scene.bin");
        addPath("scene.bin");

        // Then discover all *.bin under common scene folders
        const std::vector<fs::path> dirs = { "Scenes", "../Scenes", "." };
        for (const auto& dir : dirs) {
            std::error_code ec;
            if (!fs::exists(dir, ec) || ec || !fs::is_directory(dir, ec)) continue;

            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (ec) break;
                addPath(entry.path());
            }
        }

        return out;
    }
}
#endif

// Validation layers
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// Debug messenger functions
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, 
    VkDebugUtilsMessengerEXT debugMessenger, 
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

// Destructor
SandboxApplication::~SandboxApplication() = default;

// Vertex methods
VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
    attributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) };
    attributeDescriptions[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) };
    attributeDescriptions[2] = { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) };
    return attributeDescriptions;
}

// Main entry point
void SandboxApplication::Run() {
#ifdef _WIN32
    if (const DWORD_PTR visMask = GetVisualizationAffinityMask(); visMask != 0) {
        SetThreadAffinityMask(GetCurrentThread(), visMask);
        SetThreadDescription(GetCurrentThread(), L"VisualisationMain");
    }
#endif

    initWindow();
    initVulkan();
    initImGui();
    initScenarios();
    StartSimulationWorker();
    mainLoop();
    StopSimulationWorker();
    cleanup();
}

void SandboxApplication::ChangeScenario(int index) {
    if (index < 0 || index >= static_cast<int>(m_ScenarioFactories.size())) return;

    std::lock_guard<std::recursive_mutex> guard(m_ScenarioMutex);

    if (m_CurrentScenario) {
        m_CurrentScenario->OnUnload();
    }

    m_CurrentScenarioIndex = index;
    m_CurrentScenario = m_ScenarioFactories[index].second();
    m_CurrentScenario->OnLoad();
    m_UILayer->SetSelectedScenario(index);
}

void SandboxApplication::Stop() {
    m_IsPaused.store(true);

    std::lock_guard<std::recursive_mutex> guard(m_ScenarioMutex);
    if (m_CurrentScenario) {
        m_CurrentScenario->OnUnload();
        m_CurrentScenario->OnLoad();
    }

    m_AccumulatedTime = 0.0f;
}

void SandboxApplication::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Physics Sandbox", nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, framebufferResizeCallback);
    glfwSetCursorPosCallback(m_Window, mouseCallback);

    m_Camera.SetPosition(glm::vec3(0.0f, 5.0f, 10.0f));
}

void SandboxApplication::initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createDepthResources();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void SandboxApplication::initImGui() {
    m_UILayer = std::make_unique<ImGuiLayer>();
    m_UILayer->Init(m_Window, m_Instance, m_PhysicalDevice, m_Device,
                    m_GraphicsQueueFamily, m_GraphicsQueue, m_SwapChainImageFormat);
}

void SandboxApplication::initScenarios() {
    m_HasLoadedFlatBufferScene = false;
    m_LoadedScenes.clear();
    m_LoadedSceneWarningsByIndex.clear();
    m_LoadedSceneNames.clear();
    m_ActiveLoadedSceneIndex = -1;

    const auto sceneCandidates = CollectCandidateSceneFiles();

    for (const auto& path : sceneCandidates) {
        auto loadResult = SimRuntime::SceneLoaderFlatBuffer::LoadFromFile(path);
        if (!loadResult.success) {
            std::cerr << "[SceneLoader] Failed to load '" << path << "': " << loadResult.error << std::endl;
            continue;
        }

        std::string displayName = loadResult.scene.name;
        if (displayName.empty()) {
            displayName = std::filesystem::path(path).stem().string();
        }

        m_LoadedSceneNames.push_back(displayName);
        m_LoadedScenes.push_back(std::move(loadResult.scene));
        m_LoadedSceneWarningsByIndex.push_back(std::move(loadResult.warnings));

        const auto& loaded = m_LoadedScenes.back();
        std::cout << "[SceneLoader] Loaded scene: " << displayName
            << " | objects=" << loaded.objects.size()
            << " | spawners=" << loaded.spawners.size()
            << " | materials=" << loaded.materials.size()
            << " | cameras=" << loaded.cameras.size()
            << std::endl;

        for (const auto& warning : m_LoadedSceneWarningsByIndex.back()) {
            std::cout << "[SceneLoader][Warning] " << warning << std::endl;
        }
    }

    if (!m_LoadedScenes.empty()) {
        m_HasLoadedFlatBufferScene = true;
        m_ActiveLoadedSceneIndex = 0;
        RefreshActiveLoadedCameraCache();
    }
    else {
        m_ActiveLoadedCameraNames.clear();
        m_ActiveLoadedCameraIndex = -1;
        std::cout << "[SceneLoader] No FlatBuffer scene found yet. Using fallback preview + built-in scenarios." << std::endl;
    }

    RegisterScenario<FlatBufferPreviewScenario>("FlatBuffer Preview");
    RegisterScenario<FlockingScenario>("Flocking (Networked)");
    RegisterScenario<ClearColorScenario>("Clear Color");
    RegisterScenario<SphereDropScenario>("Sphere Drop");
    RegisterScenario<CollisionScenario>("Collision Tests");
    RegisterScenario<OrientationScenario>("Orientation Tests");

    std::vector<std::string> names;
    for (const auto& [name, factory] : m_ScenarioFactories) {
        names.push_back(name);
    }
    m_UILayer->SetScenarioNames(names);

    if (!m_ScenarioFactories.empty()) {
        ChangeScenario(0);
    }
}

void SandboxApplication::StepOnce() {
    m_IsPaused.store(true);
    m_StepRequested.store(true);
}

void SandboxApplication::mainLoop() {
    using clock = std::chrono::steady_clock;
    auto lastRender = clock::now();

    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();

        auto now = clock::now();
        float deltaTime = std::chrono::duration<float>(now - lastRender).count();

        const float renderHz = std::max(1.0f, m_RenderTickHz.load());
        const float targetFrame = 1.0f / renderHz;

        if (deltaTime < targetFrame) {
            std::this_thread::sleep_for(std::chrono::duration<float>(targetFrame - deltaTime));
            now = clock::now();
            deltaTime = std::chrono::duration<float>(now - lastRender).count();
        }

        lastRender = now;

        if (deltaTime > 0.0001f) {
            m_MeasuredRenderTickHz.store(1.0f / deltaTime);
        }

#ifdef _WIN32
        m_LastRenderCpu.store(static_cast<int>(GetCurrentProcessorNumber()));
#endif

        ProcessInput(deltaTime);
        drawFrame();
    }

    vkDeviceWaitIdle(m_Device);
}

void SandboxApplication::cleanup() {
    m_UILayer->Shutdown(m_Device);
    m_UILayer.reset();

    if (m_CurrentScenario) {
        m_CurrentScenario->OnUnload();
        m_CurrentScenario.reset();
    }

    cleanupSwapChain();

    if (m_GraphicsPipelineNoCull != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Device, m_GraphicsPipelineNoCull, nullptr);
        m_GraphicsPipelineNoCull = VK_NULL_HANDLE;
    }

    vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);

    vkDestroyBuffer(m_Device, m_IndexBuffer, nullptr);
    vkFreeMemory(m_Device, m_IndexBufferMemory, nullptr);
    vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
    vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyBuffer(m_Device, m_UniformBuffers[i], nullptr);
        vkFreeMemory(m_Device, m_UniformBuffersMemory[i], nullptr);
    }
    vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    vkDestroyDevice(m_Device, nullptr);

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    vkDestroyInstance(m_Instance, nullptr);

    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void SandboxApplication::StartSimulationWorker() {
    if (m_RunSimulationThread.exchange(true)) {
        return;
    }

    m_SimulationThread = std::thread([this]() { SimulationWorkerMain(); });

#ifdef _WIN32
    if (const DWORD_PTR simMask = GetSimulationAffinityMask(); simMask != 0) {
        SetThreadAffinityMask(
            reinterpret_cast<HANDLE>(m_SimulationThread.native_handle()),
            simMask
        );

        SetThreadDescription(
            reinterpret_cast<HANDLE>(m_SimulationThread.native_handle()),
            L"SimulationWorker"
        );
    }
#endif
}

void SandboxApplication::StopSimulationWorker() {
    if (!m_RunSimulationThread.exchange(false)) {
        return;
    }

    if (m_SimulationThread.joinable()) {
        m_SimulationThread.join();
    }
}

void SandboxApplication::SimulationWorkerMain() {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (m_RunSimulationThread.load()) {
        auto now = clock::now();
        float frameDt = std::chrono::duration<float>(now - last).count();
        last = now;

#ifdef _WIN32
        m_LastSimulationCpu.store(static_cast<int>(GetCurrentProcessorNumber()));
#endif

        const float timeStep = m_TimeStep.load();
        const float simSpeed = m_SimulationSpeed.load();

        if (!m_IsPaused.load()) {
            m_AccumulatedTime += frameDt * simSpeed;

            while (m_AccumulatedTime >= timeStep) {
                std::lock_guard<std::recursive_mutex> guard(m_ScenarioMutex);
                if (m_CurrentScenario) {
                    m_CurrentScenario->OnUpdate(timeStep);
                }
                m_AccumulatedTime -= timeStep;
            }
        }
        else if (m_StepRequested.exchange(false)) {
            std::lock_guard<std::recursive_mutex> guard(m_ScenarioMutex);
            if (m_CurrentScenario) {
                m_CurrentScenario->OnUpdate(timeStep);
            }
        }

        if (frameDt > 0.0001f) {
            m_MeasuredSimulationTickHz.store(1.0f / frameDt);
        }

        const float simHz = std::max(1.0f, m_SimulationTickHz.load());
        std::this_thread::sleep_for(std::chrono::duration<float>(1.0f / simHz));
    }
}

void SandboxApplication::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<SandboxApplication*>(glfwGetWindowUserPointer(window));
    app->m_FramebufferResized = true;
}

// ==================== VULKAN SETUP ====================

void SandboxApplication::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Physics Sandbox";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

void SandboxApplication::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;

    if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

void SandboxApplication::createSurface() {
    if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void SandboxApplication::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_PhysicalDevice = device;
            break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void SandboxApplication::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);
    m_GraphicsQueueFamily = indices.graphicsFamily.value();

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2Features sync2Features{};
    sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2Features.synchronization2 = VK_TRUE;
    dynamicRenderingFeatures.pNext = &sync2Features;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &dynamicRenderingFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void SandboxApplication::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_PhysicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_PhysicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
    m_SwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

    m_SwapChainImageFormat = surfaceFormat.format;
    m_SwapChainExtent = extent;
}

void SandboxApplication::createImageViews() {
    m_SwapChainImageViews.resize(m_SwapChainImages.size());
    for (size_t i = 0; i < m_SwapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_SwapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_SwapChainImageFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void SandboxApplication::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void SandboxApplication::createGraphicsPipeline() {
    auto vertShaderCode = readFile("glsl glsl shaders/vert.spv");
    auto fragShaderCode = readFile("glsl glsl shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    m_DepthFormat = findDepthFormat();

    VkPipelineRenderingCreateInfo renderingCreateInfo{};
    renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCreateInfo.colorAttachmentCount = 1;
    renderingCreateInfo.pColorAttachmentFormats = &m_SwapChainImageFormat;
    renderingCreateInfo.depthAttachmentFormat = m_DepthFormat; // NEW

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil; // NEW
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_PipelineLayout;

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Clone with culling disabled for container inside visibility
    VkPipelineRasterizationStateCreateInfo rasterizerNoCull = rasterizer;
    rasterizerNoCull.cullMode = VK_CULL_MODE_NONE;

    VkGraphicsPipelineCreateInfo noCullPipelineInfo = pipelineInfo;
    noCullPipelineInfo.pRasterizationState = &rasterizerNoCull;

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &noCullPipelineInfo, nullptr, &m_GraphicsPipelineNoCull) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create no-cull graphics pipeline!");
    }
    vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
}

void SandboxApplication::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_PhysicalDevice);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void SandboxApplication::createVertexBuffer() {
    // Simple quad for now
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
    };

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);
    copyBuffer(stagingBuffer, m_VertexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void SandboxApplication::createIndexBuffer() {
    std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(m_Device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), bufferSize);
    vkUnmapMemory(m_Device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);
    copyBuffer(stagingBuffer, m_IndexBuffer, bufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingBufferMemory, nullptr);
}

void SandboxApplication::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    m_UniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    m_UniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                     m_UniformBuffers[i], m_UniformBuffersMemory[i]);
        vkMapMemory(m_Device, m_UniformBuffersMemory[i], 0, bufferSize, 0, &m_UniformBuffersMapped[i]);
    }
}

void SandboxApplication::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void SandboxApplication::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    m_DescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    if (vkAllocateDescriptorSets(m_Device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
    }
}

void SandboxApplication::createCommandBuffers() {
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

    if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void SandboxApplication::createSyncObjects() {
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects!");
        }
    }
}

void SandboxApplication::drawFrame() {
    vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX,
        m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }

    m_UILayer->BeginFrame();
    m_UILayer->RenderMainMenuBar(this);
    m_UILayer->RenderControlPanel(this);

    if (m_CurrentScenario) {
        std::lock_guard<std::recursive_mutex> guard(m_ScenarioMutex);
        m_CurrentScenario->OnImGui();
    }

    updateUniformBuffer(m_CurrentFrame);

    vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);
    vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
    recordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

    VkCommandBufferSubmitInfo commandBufferInfo{};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfo.commandBuffer = m_CommandBuffers[m_CurrentFrame];

    VkSemaphoreSubmitInfo waitSemaphoreInfo{};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = m_ImageAvailableSemaphores[m_CurrentFrame];
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signalSemaphoreInfo{};
    signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfo.semaphore = m_RenderFinishedSemaphores[m_CurrentFrame];
    signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfo;

    if (vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_SwapChain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
        m_FramebufferResized = false;
        recreateSwapChain();
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void SandboxApplication::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(m_Device);
    cleanupSwapChain();
    createSwapChain();
    createImageViews();
    createDepthResources();
}

void SandboxApplication::cleanupSwapChain() {
    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }
    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_Device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }
    if (m_DepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);
        m_DepthImageMemory = VK_NULL_HANDLE;
    }

    for (auto imageView : m_SwapChainImageViews) {
        vkDestroyImageView(m_Device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

void SandboxApplication::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier2 barrierToAttachment{};
    barrierToAttachment.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrierToAttachment.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrierToAttachment.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrierToAttachment.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrierToAttachment.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierToAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierToAttachment.image = m_SwapChainImages[imageIndex];
    barrierToAttachment.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrierToAttachment;
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    glm::vec4 clearColor = m_CurrentScenario
        ? m_CurrentScenario->GetClearColor()
        : glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_SwapChainImageViews[imageIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = { { clearColor.r, clearColor.g, clearColor.b, clearColor.a } };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_DepthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, m_SwapChainExtent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment; // NEW

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_SwapChainExtent.width);
    viewport.height = static_cast<float>(m_SwapChainExtent.height);
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_SwapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    BindDefaultPipeline(commandBuffer);
    VkBuffer vertexBuffers[] = { m_VertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1,
        &m_DescriptorSets[m_CurrentFrame], 0, nullptr);

    if (m_CurrentScenario) {
        std::lock_guard<std::recursive_mutex> guard(m_ScenarioMutex);
        m_CurrentScenario->OnRender(commandBuffer);
    }
    else {
        PushConstants pushConstants{};
        pushConstants.model = glm::mat4(1.0f);
        pushConstants.checkerColorA = m_MaterialSettings.lightColor;
        pushConstants.checkerColorB = m_MaterialSettings.darkColor;
        pushConstants.checkerParams = glm::vec4(m_MaterialSettings.checkerScale, 0.0f, 0.0f, 0.0f);

        vkCmdPushConstants(commandBuffer, m_PipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
    }

    m_UILayer->EndFrame(commandBuffer);

    vkCmdEndRendering(commandBuffer);

    VkImageMemoryBarrier2 barrierToPresent{};
    barrierToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrierToPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrierToPresent.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrierToPresent.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrierToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierToPresent.image = m_SwapChainImages[imageIndex];
    barrierToPresent.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    depInfo.pImageMemoryBarriers = &barrierToPresent;
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    vkEndCommandBuffer(commandBuffer);
}

void SandboxApplication::updateUniformBuffer(uint32_t currentImage) {
    float aspect = m_SwapChainExtent.width / (float)m_SwapChainExtent.height;

    const auto* scene = GetLoadedScene();
    if (scene &&
        m_ActiveLoadedCameraIndex >= 0 &&
        m_ActiveLoadedCameraIndex < static_cast<int>(scene->cameras.size())) {

        const auto& cam = scene->cameras[static_cast<size_t>(m_ActiveLoadedCameraIndex)];

        if (cam.projection == SimRuntime::CameraProjection::Orthographic) {
            const float halfH = std::max(0.1f, cam.orthoSize);
            const float halfW = halfH * aspect;
            m_Camera.SetOrthographic(-halfW, halfW, -halfH, halfH, cam.nearPlane, cam.farPlane);
        }
        else {
            m_Camera.SetPerspective(glm::radians(cam.fov), aspect, cam.nearPlane, cam.farPlane);
        }

        glm::mat4 r(1.0f);
        r = glm::rotate(r, glm::radians(cam.transform.orientation.yaw), glm::vec3(0, 1, 0));
        r = glm::rotate(r, glm::radians(cam.transform.orientation.pitch), glm::vec3(1, 0, 0));
        r = glm::rotate(r, glm::radians(cam.transform.orientation.roll), glm::vec3(0, 0, 1));

        const glm::vec3 pos = cam.transform.position;
        const glm::vec3 fwd = glm::normalize(glm::vec3(r * glm::vec4(0, 0, -1, 0)));
        const glm::vec3 up = glm::normalize(glm::vec3(r * glm::vec4(0, 1, 0, 0)));

        m_Camera.SetLookAt(pos, pos + fwd, up);

        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view = m_Camera.GetViewMatrix();
        ubo.proj = m_Camera.GetProjectionMatrix();
        memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
        return;
    }

    if (m_UseOrthographic) {
        float halfH = m_OrthoSize;
        float halfW = m_OrthoSize * aspect;
        m_Camera.SetOrthographic(-halfW, halfW, -halfH, halfH, 0.1f, 100.0f);
    }
    else {
        m_Camera.SetPerspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }

    switch (m_CameraView) {
    case CameraView::Top:
        m_Camera.SetLookAt({ 0.0f, 12.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f });
        break;
    case CameraView::Front:
        m_Camera.SetLookAt({ 0.0f, 5.0f, 12.0f }, { 0.0f, 0.0f, 0.0f });
        break;
    case CameraView::Side:
        m_Camera.SetLookAt({ 12.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        break;
    case CameraView::Perspective:
    default:
        // Free camera
        break;
    }

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = m_Camera.GetViewMatrix();
    ubo.proj = m_Camera.GetProjectionMatrix();

    memcpy(m_UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

// ==================== HELPERS ====================

bool SandboxApplication::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails support = querySwapChainSupport(device);
        swapChainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicFeatures{};
    dynamicFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &dynamicFeatures;
    vkGetPhysicalDeviceFeatures2(device, &features2);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && dynamicFeatures.dynamicRendering;
}

bool SandboxApplication::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : available) {
        required.erase(ext.extensionName);
    }
    return required.empty();
}

QueueFamilyIndices SandboxApplication::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    int i = 0;
    for (const auto& family : families) {
        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) break;
        i++;
    }
    return indices;
}

SwapChainSupportDetails SandboxApplication::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

VkSurfaceFormatKHR SandboxApplication::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats[0];
}

VkPresentModeKHR SandboxApplication::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SandboxApplication::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);
    VkExtent2D extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

std::vector<const char*> SandboxApplication::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

bool SandboxApplication::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool found = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(layerName, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

std::vector<char> SandboxApplication::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

VkShaderModule SandboxApplication::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

void SandboxApplication::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                       VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                                       VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }
    vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
}

void SandboxApplication::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_GraphicsQueue);
    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
}

uint32_t SandboxApplication::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

VkFormat SandboxApplication::findDepthFormat()
{
    const std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported depth format!");
}

void SandboxApplication::createDepthResources() {
    m_DepthFormat = findDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_SwapChainExtent.width;
    imageInfo.extent.height = m_SwapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_DepthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_Device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image!");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(m_Device, m_DepthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth image memory!");
    }

    vkBindImageMemory(m_Device, m_DepthImage, m_DepthImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_DepthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view!");
    }
}

SandboxApplication::MeshBuffers SandboxApplication::UploadMesh(const Mesh& mesh) {
    MeshBuffers buffers{};
    buffers.indexCount = static_cast<uint32_t>(mesh.indices.size());

    // Upload vertices
    VkDeviceSize vertexBufferSize = sizeof(MeshVertex) * mesh.vertices.size();
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(m_Device, stagingMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, mesh.vertices.data(), vertexBufferSize);
    vkUnmapMemory(m_Device, stagingMemory);

    createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffers.vertexBuffer, buffers.vertexMemory);
    copyBuffer(stagingBuffer, buffers.vertexBuffer, vertexBufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingMemory, nullptr);

    // Upload indices
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * mesh.indices.size();
    createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    vkMapMemory(m_Device, stagingMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, mesh.indices.data(), indexBufferSize);
    vkUnmapMemory(m_Device, stagingMemory);

    createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffers.indexBuffer, buffers.indexMemory);
    copyBuffer(stagingBuffer, buffers.indexBuffer, indexBufferSize);

    vkDestroyBuffer(m_Device, stagingBuffer, nullptr);
    vkFreeMemory(m_Device, stagingMemory, nullptr);

    return buffers;
}

void SandboxApplication::DestroyMeshBuffers(const MeshBuffers& buffers) {
    vkDestroyBuffer(m_Device, buffers.vertexBuffer, nullptr);
    vkFreeMemory(m_Device, buffers.vertexMemory, nullptr);
    vkDestroyBuffer(m_Device, buffers.indexBuffer, nullptr);
    vkFreeMemory(m_Device, buffers.indexMemory, nullptr);
}

void SandboxApplication::BindDefaultPipeline(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
}

void SandboxApplication::BindNoCullPipeline(VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipelineNoCull);
}

void SandboxApplication::ProcessInput(float deltaTime)
{
    if (IsUsingLoadedCamera()) {
        return; // loaded camera is authoritative
    }

    // Toggle camera control with right mouse button
    if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (!m_CameraEnabled) {
            // Just enabled, reset mouse position
            double xpos, ypos;
            glfwGetCursorPos(m_Window, &xpos, &ypos);
            m_LastMouseX = xpos;
            m_LastMouseY = ypos;
            m_FirstMouse = true;
        }
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_CameraEnabled = true;
    }
    else {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        m_CameraEnabled = false;
        return;  // Don't process camera movement when disabled
    }

    // Camera movement (only executes when right mouse is held)
    if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(0, deltaTime);
    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(1, deltaTime);
    if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(2, deltaTime);
    if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(3, deltaTime);
    if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(4, deltaTime);
    if (glfwGetKey(m_Window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        m_Camera.ProcessKeyboard(5, deltaTime);
}

bool SandboxApplication::HasLoadedFlatBufferScene() const {
    return m_HasLoadedFlatBufferScene && m_ActiveLoadedSceneIndex >= 0 &&
        m_ActiveLoadedSceneIndex < static_cast<int>(m_LoadedScenes.size());
}

const SimRuntime::SceneRuntime* SandboxApplication::GetLoadedScene() const {
    if (!HasLoadedFlatBufferScene()) return nullptr;
    return &m_LoadedScenes[static_cast<size_t>(m_ActiveLoadedSceneIndex)];
}

const std::vector<std::string>& SandboxApplication::GetLoadedSceneWarnings() const {
    static const std::vector<std::string> empty;
    if (!HasLoadedFlatBufferScene()) return empty;
    return m_LoadedSceneWarningsByIndex[static_cast<size_t>(m_ActiveLoadedSceneIndex)];
}

const std::vector<std::string>& SandboxApplication::GetLoadedSceneNames() const {
    return m_LoadedSceneNames;
}

int SandboxApplication::GetActiveLoadedSceneIndex() const {
    return m_ActiveLoadedSceneIndex;
}

bool SandboxApplication::SetActiveLoadedSceneIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_LoadedScenes.size())) {
        return false;
    }
    m_ActiveLoadedSceneIndex = index;
    m_HasLoadedFlatBufferScene = true;
    RefreshActiveLoadedCameraCache();
    return true;
}

const std::vector<std::string>& SandboxApplication::GetLoadedCameraNames() const {
    return m_ActiveLoadedCameraNames;
}

int SandboxApplication::GetActiveLoadedCameraIndex() const {
    return m_ActiveLoadedCameraIndex;
}

bool SandboxApplication::SetActiveLoadedCameraIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_ActiveLoadedCameraNames.size())) {
        return false;
    }
    m_ActiveLoadedCameraIndex = index;
    return true;
}

void SandboxApplication::ClearActiveLoadedCameraSelection() {
    m_ActiveLoadedCameraIndex = -1;
}

void SandboxApplication::RefreshActiveLoadedCameraCache() {
    m_ActiveLoadedCameraNames.clear();
    m_ActiveLoadedCameraIndex = -1;

    const auto* scene = GetLoadedScene();
    if (!scene) return;

    for (size_t i = 0; i < scene->cameras.size(); ++i) {
        const auto& c = scene->cameras[i];
        m_ActiveLoadedCameraNames.push_back(
            c.name.empty() ? ("Camera " + std::to_string(i + 1)) : c.name
        );
    }
}

void SandboxApplication::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    auto app = reinterpret_cast<SandboxApplication*>(glfwGetWindowUserPointer(window));

    if (!app->m_CameraEnabled) return;

    if (app->m_FirstMouse) {
        app->m_LastMouseX = xpos;
        app->m_LastMouseY = ypos;
        app->m_FirstMouse = false;
        return;  // Skip first frame to avoid jump
    }

    float xoffset = static_cast<float>(xpos - app->m_LastMouseX);
    float yoffset = static_cast<float>(app->m_LastMouseY - ypos);

    app->m_LastMouseX = xpos;
    app->m_LastMouseY = ypos;

    app->m_Camera.ProcessMouseMovement(xoffset, yoffset);
}