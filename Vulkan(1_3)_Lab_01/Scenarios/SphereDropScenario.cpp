#include "SphereDropScenario.h"
#include "../Application/SandboxApplication.h"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

void SphereDropScenario::OnLoad() {
    m_SpherePosition = {0.0f, 5.0f, 0.0f};
    m_Velocity = {0.0f, 0.0f, 0.0f};

    // Generate meshes
    m_SphereMesh = MeshGenerator::GenerateSphere(m_Radius, 32, 16, {1.0f, 0.3f, 0.3f});
    m_GroundMesh = MeshGenerator::GeneratePlane(10.0f, 10.0f, 10, 10, {0.4f, 0.4f, 0.4f});

    // Upload to GPU
    m_SphereBuffers = m_App->UploadMesh(m_SphereMesh);
    m_GroundBuffers = m_App->UploadMesh(m_GroundMesh);
}

void SphereDropScenario::OnUpdate(float deltaTime) {
    m_Velocity.y += m_Gravity * deltaTime;
    m_SpherePosition += m_Velocity * deltaTime;

    if (m_SpherePosition.y - m_Radius < m_GroundY) {
        m_SpherePosition.y = m_GroundY + m_Radius;
        m_Velocity.y = -m_Velocity.y * 0.8f;
        
        if (glm::abs(m_Velocity.y) < 0.01f) {
            m_Velocity.y = 0.0f;
        }
    }
}

void SphereDropScenario::OnRender(VkCommandBuffer commandBuffer) {
    // Render ground plane
    glm::mat4 groundModel = glm::mat4(1.0f);
    vkCmdPushConstants(commandBuffer,
        m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(glm::mat4), &groundModel);

    VkBuffer vertexBuffers[] = { m_GroundBuffers.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_GroundBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_GroundBuffers.indexCount, 1, 0, 0, 0);

    // Render sphere at dynamic position
    glm::mat4 sphereModel = glm::translate(glm::mat4(1.0f), m_SpherePosition);
    vkCmdPushConstants(commandBuffer,
        m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof(glm::mat4), &sphereModel);

    vertexBuffers[0] = m_SphereBuffers.vertexBuffer;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_SphereBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_SphereBuffers.indexCount, 1, 0, 0, 0);
}

void SphereDropScenario::OnUnload() {
    m_App->DestroyMeshBuffers(m_SphereBuffers);
    m_App->DestroyMeshBuffers(m_GroundBuffers);
}

void SphereDropScenario::OnImGui() {
    ImGui::Begin("Sphere Drop Settings");
    
    ImGui::Text("Sphere Properties");
    ImGui::DragFloat3("Position", &m_SpherePosition.x, 0.1f);
    ImGui::DragFloat3("Velocity", &m_Velocity.x, 0.1f);
    ImGui::SliderFloat("Radius", &m_Radius, 0.1f, 2.0f);
    
    ImGui::Separator();
    ImGui::Text("Physics");
    ImGui::SliderFloat("Gravity", &m_Gravity, -20.0f, 0.0f);
    ImGui::DragFloat("Ground Y", &m_GroundY, 0.1f, -5.0f, 5.0f);
    
    ImGui::Separator();
    if (ImGui::Button("Reset Sphere")) {
        OnLoad();
    }
    
    ImGui::End();
}
