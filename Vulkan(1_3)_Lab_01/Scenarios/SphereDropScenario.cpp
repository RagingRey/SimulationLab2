#include "SphereDropScenario.h"
#include "../Application/SandboxApplication.h"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

void SphereDropScenario::OnLoad() {
    m_SpherePosition = { 0.0f, 5.0f, 0.0f };
    m_Velocity = { 0.0f, 0.0f, 0.0f };

    m_SphereBPosition = { 2.0f, 7.0f, 0.0f };
    m_SphereBVelocity = { 0.0f, 0.0f, 0.0f };

    m_SphereObject.SetPosition(m_SpherePosition);
    m_SphereBObject.SetPosition(m_SphereBPosition);
    m_GroundObject.SetTransform(glm::mat4(1.0f));

    m_SphereMesh = MeshGenerator::GenerateSphere(m_Radius, 32, 16, { 1.0f, 0.3f, 0.3f });
    m_GroundMesh = MeshGenerator::GeneratePlane(10.0f, 10.0f, 10, 10, { 0.4f, 0.4f, 0.4f });

    m_SphereBuffers = m_App->UploadMesh(m_SphereMesh);
    m_GroundBuffers = m_App->UploadMesh(m_GroundMesh);
}

void SphereDropScenario::OnUpdate(float deltaTime) {
    auto integrate = [&](glm::vec3& position, glm::vec3& velocity) {
        velocity.y += m_Gravity * deltaTime;
        position += velocity * deltaTime;

        if (position.y - m_Radius < m_GroundY) {
            position.y = m_GroundY + m_Radius;
            velocity.y = -velocity.y * 0.8f;

            if (glm::abs(velocity.y) < 0.01f) {
                velocity.y = 0.0f;
            }
        }
        };

    integrate(m_SpherePosition, m_Velocity);
    integrate(m_SphereBPosition, m_SphereBVelocity);

    m_SphereObject.SetPosition(m_SpherePosition);
    m_SphereBObject.SetPosition(m_SphereBPosition);
}

void SphereDropScenario::OnRender(VkCommandBuffer commandBuffer) {
    auto& material = m_App->GetMaterialSettings();

    // Ground
    PushConstants groundPush{};
    groundPush.model = m_GroundObject.GetTransform();
    groundPush.checkerColorA = material.lightColor;
    groundPush.checkerColorB = material.darkColor;
    groundPush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

    vkCmdPushConstants(commandBuffer,
        m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &groundPush);

    VkBuffer vertexBuffers[] = { m_GroundBuffers.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_GroundBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_GroundBuffers.indexCount, 1, 0, 0, 0);

    // Sphere A
    PushConstants spherePush{};
    spherePush.model = m_SphereObject.GetTransform();
    spherePush.checkerColorA = material.lightColor;
    spherePush.checkerColorB = material.darkColor;
    spherePush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

    vkCmdPushConstants(commandBuffer,
        m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &spherePush);

    vertexBuffers[0] = m_SphereBuffers.vertexBuffer;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_SphereBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_SphereBuffers.indexCount, 1, 0, 0, 0);

    // Sphere B
    PushConstants sphereBPush{};
    sphereBPush.model = m_SphereBObject.GetTransform();
    sphereBPush.checkerColorA = material.lightColor;
    sphereBPush.checkerColorB = material.darkColor;
    sphereBPush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

    vkCmdPushConstants(commandBuffer,
        m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof(PushConstants), &sphereBPush);

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

void SphereDropScenario::ImGuiMainMenu() {
    if (ImGui::BeginMenu("Sphere")) {
        if (ImGui::MenuItem("Reset")) {
            OnLoad();
        }
        ImGui::Separator();
        ImGui::SliderFloat("Gravity", &m_Gravity, -20.0f, 0.0f);
        ImGui::EndMenu();
    }
}
