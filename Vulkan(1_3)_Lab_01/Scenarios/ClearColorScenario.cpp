#include "ClearColorScenario.h"
#include <imgui.h>

void ClearColorScenario::OnImGui() {
    ImGui::Begin("Clear Color Settings");
    ImGui::ColorEdit4("Background", &m_ClearColor.x);
    ImGui::End();
}

void ClearColorScenario::ImGuiMainMenu() {
    if (ImGui::BeginMenu("Colour")) {
        ImGui::ColorEdit4("Background", &m_ClearColor.x);
        ImGui::EndMenu();
    }
}