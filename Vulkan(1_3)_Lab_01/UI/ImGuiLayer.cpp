#include "ImGuiLayer.h"
#include "../Application/SandboxApplication.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <stdexcept>

void ImGuiLayer::Init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice,
                      VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue,
                      VkFormat swapChainFormat) {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = graphicsQueueFamily;
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = m_ImGuiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;

    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainFormat;

    ImGui_ImplVulkan_Init(&initInfo);
    m_Initialized = true;
}

void ImGuiLayer::Shutdown(VkDevice device) {
    if (!m_Initialized) return;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(device, m_ImGuiDescriptorPool, nullptr);
    m_Initialized = false;
}

void ImGuiLayer::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiLayer::RenderMainMenuBar(SandboxApplication* app) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("Scenario")) {
        for (int i = 0; i < static_cast<int>(m_ScenarioNames.size()); i++) {
            bool isSelected = (m_SelectedScenarioIndex == i);
            if (ImGui::MenuItem(m_ScenarioNames[i].c_str(), nullptr, isSelected)) {
                m_SelectedScenarioIndex = i;
                app->ChangeScenario(i);
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Camera")) {
        if (ImGui::MenuItem("Free (Perspective)", nullptr, app->GetCameraView() == CameraView::Perspective)) {
            app->SetCameraView(CameraView::Perspective);
        }
        if (ImGui::MenuItem("Top", nullptr, app->GetCameraView() == CameraView::Top)) {
            app->SetCameraView(CameraView::Top);
        }
        if (ImGui::MenuItem("Front", nullptr, app->GetCameraView() == CameraView::Front)) {
            app->SetCameraView(CameraView::Front);
        }
        if (ImGui::MenuItem("Side", nullptr, app->GetCameraView() == CameraView::Side)) {
            app->SetCameraView(CameraView::Side);
        }

        ImGui::Separator();
        bool ortho = app->IsOrthographic();
        if (ImGui::Checkbox("Orthographic", &ortho)) {
            app->SetOrthographic(ortho);
        }
        float orthoSize = app->GetOrthoSize();
        if (ImGui::SliderFloat("Ortho Size", &orthoSize, 2.0f, 20.0f)) {
            app->SetOrthoSize(orthoSize);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Simulation")) {
        if (ImGui::MenuItem(app->IsPaused() ? "Start" : "Pause")) {
            app->TogglePause();
        }
        if (ImGui::MenuItem("Step")) {
            app->StepOnce();
        }

        float timeStep = app->GetTimeStep();
        if (ImGui::SliderFloat("Time Step", &timeStep, 0.001f, 0.1f, "%.4f s")) {
            app->SetTimeStep(timeStep);
        }
        ImGui::EndMenu();
    }

    if (auto* scenario = app->GetCurrentScenario()) {
        scenario->ImGuiMainMenu();
    }

    ImGui::EndMainMenuBar();
}

void ImGuiLayer::RenderControlPanel(SandboxApplication* app) {
    ImGui::Begin("Simulation Controls");

    const char* currentName = m_ScenarioNames.empty() ? "None"
                              : m_ScenarioNames[m_SelectedScenarioIndex].c_str();

    if (ImGui::BeginCombo("Scenario", currentName)) {
        for (int i = 0; i < static_cast<int>(m_ScenarioNames.size()); i++) {
            bool isSelected = (m_SelectedScenarioIndex == i);
            if (ImGui::Selectable(m_ScenarioNames[i].c_str(), isSelected)) {
                m_SelectedScenarioIndex = i;
                app->ChangeScenario(i);
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::Text("Playback");

    ImGui::Separator();
    ImGui::Text("Camera Controls");
    ImGui::Text("Hold RIGHT MOUSE to look around");
    ImGui::Text("WASD: Move | Space/Ctrl: Up/Down");

    if (ImGui::Button(app->IsPaused() ? "Play" : "Pause", ImVec2(60, 0))) {
        app->TogglePause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(60, 0))) {
        app->Stop();
    }

    ImGui::Separator();
    ImGui::Text("Simulation");

    float timeStep = app->GetTimeStep();
    if (ImGui::SliderFloat("Time Step", &timeStep, 0.001f, 0.1f, "%.4f s")) {
        app->SetTimeStep(timeStep);
    }

    float simSpeed = app->GetSimulationSpeed();
    if (ImGui::SliderFloat("Speed", &simSpeed, 0.1f, 5.0f, "%.1fx")) {
        app->SetSimulationSpeed(simSpeed);
    }

    ImGui::Separator();
    ImGui::Text("Stats");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

    ImGui::End();
}