#include "OrientationScenario.h"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

void OrientationScenario::BuildTests() {
    m_Tests = {
        { "X 90",  { 90.0f, 0.0f, 0.0f } },
        { "X 180", { 180.0f, 0.0f, 0.0f } },
        { "X 270", { 270.0f, 0.0f, 0.0f } },
        { "X 360", { 360.0f, 0.0f, 0.0f } },

        { "Y 90",  { 0.0f, 90.0f, 0.0f } },
        { "Y 180", { 0.0f, 180.0f, 0.0f } },
        { "Y 270", { 0.0f, 270.0f, 0.0f } },
        { "Y 360", { 0.0f, 360.0f, 0.0f } },

        { "Z 90",  { 0.0f, 0.0f, 90.0f } },
        { "Z 180", { 0.0f, 0.0f, 180.0f } },
        { "Z 270", { 0.0f, 0.0f, 270.0f } },
        { "Z 360", { 0.0f, 0.0f, 360.0f } },

        { "XYZ Combo", { 45.0f, 30.0f, 60.0f } }
    };
}

void OrientationScenario::BuildAngularVelocityTests() {
    m_AngularVelocityTests = {
        { "X 90 deg/s for 1s", { 90.0f, 0.0f, 0.0f }, 1.0f },
        { "X 90 deg/s for 2s", { 90.0f, 0.0f, 0.0f }, 2.0f },
        { "X 90 deg/s for 3s", { 90.0f, 0.0f, 0.0f }, 3.0f },
        { "X 90 deg/s for 4s", { 90.0f, 0.0f, 0.0f }, 4.0f },

        { "Y 180 deg/s for 1s", { 0.0f, 180.0f, 0.0f }, 1.0f },
        { "Y 270 deg/s for 1s", { 0.0f, 270.0f, 0.0f }, 1.0f },
        { "Z 360 deg/s for 1s", { 0.0f, 0.0f, 360.0f }, 1.0f },

        { "Combo (90,180,270) deg/s for 1s", { 90.0f, 180.0f, 270.0f }, 1.0f }
    };
}

void OrientationScenario::ApplyDisplacementDegrees(const glm::vec3& degrees) {
    m_Object.ApplyAngularDisplacementEuler(glm::radians(degrees));
}

bool OrientationScenario::IsCardinal(const glm::vec3& axis, float tolerance) const {
    const glm::vec3 a = glm::abs(axis);
    const float maxComp = std::max(a.x, std::max(a.y, a.z));
    const bool oneIsOne = std::abs(maxComp - 1.0f) <= tolerance;
    const bool othersNearZero =
        (a.x <= tolerance || std::abs(a.x - 1.0f) <= tolerance) &&
        (a.y <= tolerance || std::abs(a.y - 1.0f) <= tolerance) &&
        (a.z <= tolerance || std::abs(a.z - 1.0f) <= tolerance);
    return oneIsOne && othersNearZero;
}

void OrientationScenario::RunAngularVelocityTest(const AngularVelocityTest& test) {
    m_Object.SetOrientationEuler({ 0.0f, 0.0f, 0.0f });
    m_Object.SetAngularVelocity(glm::radians(test.degreesPerSecond));

    float elapsed = 0.0f;
    const float dt = std::max(0.0001f, m_App->GetTimeStep());
    while (elapsed < test.durationSeconds) {
        const float step = std::min(dt, test.durationSeconds - elapsed);
        m_Object.IntegrateAngularVelocity(step);
        elapsed += step;
    }

    m_Object.SetAngularVelocity(glm::vec3(0.0f));

    glm::mat3 r = glm::mat3(m_Object.GetTransform());
    glm::vec3 right = glm::normalize(r * glm::vec3(1, 0, 0));
    glm::vec3 up = glm::normalize(r * glm::vec3(0, 1, 0));
    glm::vec3 forward = glm::normalize(r * glm::vec3(0, 0, 1));

    const bool pass = IsCardinal(right) && IsCardinal(up) && IsCardinal(forward);

    std::ostringstream oss;
    oss << test.name << " -> "
        << (pass ? "PASS" : "CHECK")
        << " | R=(" << std::fixed << std::setprecision(2) << right.x << "," << right.y << "," << right.z << ")"
        << " U=(" << up.x << "," << up.y << "," << up.z << ")"
        << " F=(" << forward.x << "," << forward.y << "," << forward.z << ")";
    m_LastAngularVelocityResult = oss.str();
}

void OrientationScenario::OnLoad() {
    BuildTests();
    BuildAngularVelocityTests();

    m_Object.SetPosition({ 0.0f, 1.0f, 0.0f });
    m_Object.SetOrientationEuler({ 0.0f, 0.0f, 0.0f });
    m_Object.SetAngularVelocity(glm::vec3(0.0f));

    m_Ground.SetTransform(glm::mat4(1.0f));
    m_Ground.SetPosition({ 0.0f, 0.0f, 0.0f });

    m_ObjectMesh = MeshGenerator::GenerateCylinder(0.35f, 1.6f, 24, { 0.2f, 0.8f, 1.0f });
    m_GroundMesh = MeshGenerator::GeneratePlane(10.0f, 10.0f, 10, 10, { 0.4f, 0.4f, 0.4f });

    m_ObjectBuffers = m_App->UploadMesh(m_ObjectMesh);
    m_GroundBuffers = m_App->UploadMesh(m_GroundMesh);
}

void OrientationScenario::OnUpdate(float deltaTime) {
    if (m_AutoRotate) {
        m_Object.SetAngularVelocity(glm::radians(m_AngularVelocityDegreesPerSecond));
    } else {
        m_Object.SetAngularVelocity(glm::vec3(0.0f));
    }

    m_Object.IntegrateAngularVelocity(deltaTime);
}

void OrientationScenario::OnRender(VkCommandBuffer commandBuffer) {
    auto& material = m_App->GetMaterialSettings();

    PushConstants groundPush{};
    groundPush.model = m_Ground.GetTransform();
    groundPush.checkerColorA = material.lightColor;
    groundPush.checkerColorB = material.darkColor;
    groundPush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

    vkCmdPushConstants(commandBuffer, m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &groundPush);

    VkBuffer vertexBuffers[] = { m_GroundBuffers.vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_GroundBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_GroundBuffers.indexCount, 1, 0, 0, 0);

    PushConstants objectPush{};
    objectPush.model = m_Object.GetTransform();
    objectPush.checkerColorA = material.lightColor;
    objectPush.checkerColorB = material.darkColor;
    objectPush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

    vkCmdPushConstants(commandBuffer, m_App->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &objectPush);

    vertexBuffers[0] = m_ObjectBuffers.vertexBuffer;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_ObjectBuffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_ObjectBuffers.indexCount, 1, 0, 0, 0);
}

void OrientationScenario::OnImGui() {
    ImGui::Begin("Orientation Tests");

    ImGui::Text("Q1: Angular Displacement");
    ImGui::DragFloat3("Delta Degrees", &m_AngularDisplacementDegrees.x, 1.0f);
    if (ImGui::Button("Apply Displacement")) {
        ApplyDisplacementDegrees(m_AngularDisplacementDegrees);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Orientation")) {
        m_Object.SetOrientationEuler({ 0.0f, 0.0f, 0.0f });
    }

    ImGui::Separator();
    ImGui::Text("Q1 Preset Displacement Tests");
    std::vector<const char*> names;
    names.reserve(m_Tests.size());
    for (auto& t : m_Tests) {
        names.push_back(t.name.c_str());
    }
    ImGui::Combo("Displacement Test", &m_SelectedTest, names.data(), static_cast<int>(names.size()));
    if (ImGui::Button("Run Displacement Test")) {
        m_Object.SetOrientationEuler({ 0.0f, 0.0f, 0.0f });
        ApplyDisplacementDegrees(m_Tests[m_SelectedTest].degrees);
    }

    ImGui::Separator();
    ImGui::Text("Q2: Angular Velocity (deg/s)");
    ImGui::Checkbox("Auto Rotate", &m_AutoRotate);
    ImGui::DragFloat3("Angular Velocity (deg/s)", &m_AngularVelocityDegreesPerSecond.x, 1.0f);
    ImGui::DragFloat("Duration (s)", &m_TestDurationSeconds, 0.1f, 0.1f, 10.0f);

    if (ImGui::Button("Run Manual Angular Velocity Test")) {
        AngularVelocityTest manual{
            "Manual",
            m_AngularVelocityDegreesPerSecond,
            m_TestDurationSeconds
        };
        RunAngularVelocityTest(manual);
    }

    ImGui::Separator();
    std::vector<const char*> velocityNames;
    velocityNames.reserve(m_AngularVelocityTests.size());
    for (auto& t : m_AngularVelocityTests) {
        velocityNames.push_back(t.name.c_str());
    }

    ImGui::Combo("Q2 Preset Angular Velocity Test", &m_SelectedAngularVelocityTest,
        velocityNames.data(), static_cast<int>(velocityNames.size()));

    if (ImGui::Button("Run Selected Angular Velocity Test")) {
        RunAngularVelocityTest(m_AngularVelocityTests[m_SelectedAngularVelocityTest]);
    }

    ImGui::TextWrapped("%s", m_LastAngularVelocityResult.c_str());

    glm::mat3 r = glm::mat3(m_Object.GetTransform());
    glm::vec3 right = glm::normalize(r * glm::vec3(1, 0, 0));
    glm::vec3 up = glm::normalize(r * glm::vec3(0, 1, 0));
    glm::vec3 forward = glm::normalize(r * glm::vec3(0, 0, 1));

    ImGui::Separator();
    ImGui::Text("Basis (world)");
    ImGui::Text("Right:   (%.3f, %.3f, %.3f)", right.x, right.y, right.z);
    ImGui::Text("Up:      (%.3f, %.3f, %.3f)", up.x, up.y, up.z);
    ImGui::Text("Forward: (%.3f, %.3f, %.3f)", forward.x, forward.y, forward.z);

    ImGui::End();
}

void OrientationScenario::OnUnload() {
    m_App->DestroyMeshBuffers(m_ObjectBuffers);
    m_App->DestroyMeshBuffers(m_GroundBuffers);
}