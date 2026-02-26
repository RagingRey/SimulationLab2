#include "SphereDropScenario.h"
#include "../Application/SandboxApplication.h"
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {
    std::string Trim(std::string value) {
        auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char c) { return !isSpace(c); }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
        return value;
    }

    bool ParseVec3(const std::string& text, glm::vec3& out) {
        std::string cleaned = text;
        std::replace(cleaned.begin(), cleaned.end(), ',', ' ');
        std::istringstream stream(cleaned);
        return static_cast<bool>(stream >> out.x >> out.y >> out.z);
    }

    bool ParseVec2(const std::string& text, glm::vec2& out) {
        std::string cleaned = text;
        std::replace(cleaned.begin(), cleaned.end(), ',', ' ');
        std::istringstream stream(cleaned);
        return static_cast<bool>(stream >> out.x >> out.y);
    }
}

void SphereDropScenario::ClearScene() {
    for (auto& sphere : m_Spheres) {
        m_App->DestroyMeshBuffers(sphere.buffers);
    }
    for (auto& plane : m_Planes) {
        m_App->DestroyMeshBuffers(plane.buffers);
    }
    m_Spheres.clear();
    m_Planes.clear();
    m_GroundPlaneIndex = -1;
}

void SphereDropScenario::RebuildSphereMesh(SphereInstance& sphere) {
    m_App->DestroyMeshBuffers(sphere.buffers);
    sphere.mesh = MeshGenerator::GenerateSphere(sphere.body.GetRadius(), 32, 16, sphere.color);
    sphere.buffers = m_App->UploadMesh(sphere.mesh);
}

void SphereDropScenario::UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position) {
    glm::mat4 transform = glm::mat4_cast(plane.orientation);
    transform[3] = glm::vec4(position, 1.0f);
    plane.body.SetTransform(transform);
}

void SphereDropScenario::ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane) {
    auto* sphereCollider = sphere.body.GetColliderAs<SphereCollider>();
    if (!sphereCollider) {
        return;
    }

    float distance = plane.DistanceToPoint(sphereCollider->GetCenter());
    float penetration = sphereCollider->GetRadius() - distance;

    if (penetration > 0.0f) {
        glm::vec3 normal = plane.GetNormal();
        glm::vec3 position = sphere.body.GetPosition();
        position += normal * penetration;
        sphere.body.SetPosition(position);

        glm::vec3 velocity = sphere.body.GetVelocity();
        float normalVelocity = glm::dot(velocity, normal);

        if (normalVelocity < 0.0f) {
            if (m_UseBounce) {
                float restitution = sphere.body.GetRestitution();
                velocity = velocity - (1.0f + restitution) * normalVelocity * normal;
            }
            else {
                velocity = glm::vec3(0.0f);
            }
        }

        glm::vec3 tangent = velocity - normal * glm::dot(velocity, normal);
        velocity -= tangent * std::clamp(m_PlaneFriction, 0.0f, 1.0f);

        if (glm::length(velocity) < m_StopSpeed) {
            velocity = glm::vec3(0.0f);
        }

        sphere.body.SetVelocity(velocity);
    }
}

void SphereDropScenario::ResolveSphereSphere(SphereInstance& a, SphereInstance& b) {
    auto* aCollider = a.body.GetColliderAs<SphereCollider>();
    auto* bCollider = b.body.GetColliderAs<SphereCollider>();
    if (!aCollider || !bCollider) {
        return;
    }

    glm::vec3 delta = bCollider->GetCenter() - aCollider->GetCenter();
    float distance = glm::length(delta);
    float radiusSum = aCollider->GetRadius() + bCollider->GetRadius();

    if (distance <= 0.0f || distance >= radiusSum) {
        return;
    }

    glm::vec3 normal = delta / distance;
    float penetration = radiusSum - distance;

    float invA = a.body.GetInverseMass();
    float invB = b.body.GetInverseMass();
    float invSum = invA + invB;

    if (invSum > 0.0f) {
        glm::vec3 correction = normal * (penetration / invSum);
        a.body.SetPosition(a.body.GetPosition() - correction * invA);
        b.body.SetPosition(b.body.GetPosition() + correction * invB);
    }

    if (m_UseBounce) {
        glm::vec3 va = a.body.GetVelocity();
        glm::vec3 vb = b.body.GetVelocity();

        float vaN = glm::dot(va, normal);
        float vbN = glm::dot(vb, -normal);

        if (vaN > 0.0f) {
            va = va - (1.0f + a.body.GetRestitution()) * vaN * normal;
        }
        if (vbN > 0.0f) {
            vb = vb - (1.0f + b.body.GetRestitution()) * vbN * -normal;
        }

        a.body.SetVelocity(va);
        b.body.SetVelocity(vb);
    } else {
        a.body.SetVelocity(glm::vec3(0.0f));
        b.body.SetVelocity(glm::vec3(0.0f));
    }
}

void SphereDropScenario::AddSphere(const glm::vec3& position,
                                   const glm::vec3& velocity,
                                   float radius,
                                   const glm::vec3& color,
                                   float restitution,
                                   float mass) {
    SphereInstance instance{};
    instance.color = color;

    instance.body.SetCollider(std::make_unique<SphereCollider>(radius));
    instance.body.SetPosition(position);
    instance.body.SetVelocity(velocity);
    instance.body.SetRadius(radius);
    instance.body.SetRestitution(restitution);
    instance.body.SetMass(mass);

    instance.mesh = MeshGenerator::GenerateSphere(radius, 32, 16, color);
    instance.buffers = m_App->UploadMesh(instance.mesh);
    m_Spheres.push_back(std::move(instance));
}

void SphereDropScenario::AddPlane(const glm::vec3& point,
                                  const glm::vec3& normal,
                                  const glm::vec2& size,
                                  const glm::vec3& color,
                                  bool isGround) {
    PlaneInstance instance{};
    instance.color = color;
    instance.size = size;

    glm::vec3 n = glm::normalize(normal);
    instance.orientation = glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), n);

    instance.body.SetCollider(std::make_unique<PlaneCollider>(n));
    instance.body.SetMass(0.0f);
    UpdatePlaneTransform(instance, point);

    instance.mesh = MeshGenerator::GeneratePlane(size.x, size.y, 10, 10, color);
    instance.buffers = m_App->UploadMesh(instance.mesh);

    if (isGround) {
        m_GroundPlaneIndex = static_cast<int>(m_Planes.size());
    }

    m_Planes.push_back(std::move(instance));
}

bool SphereDropScenario::LoadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    bool hasPlane = false;

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto splitPos = line.find('=');
        if (splitPos == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, splitPos));
        const std::string value = Trim(line.substr(splitPos + 1));

        if (key == "gravity") {
            m_Gravity = std::stof(value);
        } else if (key == "groundY") {
            m_GroundY = std::stof(value);
        } else if (key == "plane") {
            std::vector<std::string> parts;
            std::string part;
            std::istringstream partsStream(value);
            while (std::getline(partsStream, part, '|')) {
                parts.push_back(Trim(part));
            }

            if (parts.size() < 3) {
                continue;
            }

            glm::vec3 point{};
            glm::vec3 normal{0.0f, 1.0f, 0.0f};
            glm::vec2 size{10.0f, 10.0f};
            glm::vec3 color{0.4f, 0.4f, 0.4f};

            if (!ParseVec3(parts[0], point) || !ParseVec3(parts[1], normal)) {
                continue;
            }

            ParseVec2(parts[2], size);

            if (parts.size() >= 4) {
                ParseVec3(parts[3], color);
            }

            AddPlane(point, normal, size, color, false);
            hasPlane = true;
        } else if (key == "sphere") {
            std::vector<std::string> parts;
            std::string part;
            std::istringstream partsStream(value);
            while (std::getline(partsStream, part, '|')) {
                parts.push_back(Trim(part));
            }

            if (parts.size() < 3) {
                continue;
            }

            glm::vec3 position{};
            glm::vec3 velocity{};
            glm::vec3 color{1.0f, 0.3f, 0.3f};
            float radius = std::stof(parts[2]);
            float restitution = 0.8f;
            float mass = 1.0f;

            if (!ParseVec3(parts[0], position) || !ParseVec3(parts[1], velocity)) {
                continue;
            }

            if (parts.size() >= 4) {
                ParseVec3(parts[3], color);
            }

            if (parts.size() >= 5) {
                restitution = std::stof(parts[4]);
            }

            if (parts.size() >= 6) {
                mass = std::stof(parts[5]);
            }

            AddSphere(position, velocity, radius, color, restitution, mass);
        }
    }

    if (!hasPlane) {
        AddPlane({0.0f, m_GroundY, 0.0f}, {0.0f, 1.0f, 0.0f}, {10.0f, 10.0f}, {0.4f, 0.4f, 0.4f}, true);
    }

    return !m_Spheres.empty();
}

void SphereDropScenario::LoadDefaultConfig() {
    m_Gravity = -9.81f;
    m_GroundY = 0.0f;

    AddPlane({0.0f, m_GroundY, 0.0f}, {0.0f, 1.0f, 0.0f}, {10.0f, 10.0f}, {0.4f, 0.4f, 0.4f}, true);
    AddPlane({0.0f, 1.5f, -2.0f}, {0.0f, 0.707f, 0.707f}, {6.0f, 6.0f}, {0.2f, 0.5f, 0.2f}, false);

    AddSphere({ 0.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, { 1.0f, 0.3f, 0.3f }, 0.8f, 1.0f);
    AddSphere({ 2.0f, 1.0f, 0.0f }, { -2.0f, 0.0f, 0.0f }, 0.5f, { 0.3f, 0.3f, 1.0f }, 0.8f, 1.0f);
    AddSphere({ 4.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.5f, { 0.3f, 1.0f, 0.3f }, 0.8f, 0.0f);
}

void SphereDropScenario::OnLoad() {
    ClearScene();

    if (!LoadConfig(m_ConfigPath)) {
        LoadDefaultConfig();
    }
}

void SphereDropScenario::OnUpdate(float deltaTime) {
    if (m_GroundPlaneIndex >= 0 && m_GroundPlaneIndex < static_cast<int>(m_Planes.size())) {
        UpdatePlaneTransform(m_Planes[m_GroundPlaneIndex], {0.0f, m_GroundY, 0.0f});
    }

    const auto method = m_App->GetIntegrationMethod();

    for (auto& sphere : m_Spheres) {
        sphere.body.Update(deltaTime, m_Gravity, method);

        for (const auto& plane : m_Planes) {
            auto* planeCollider = plane.body.GetColliderAs<PlaneCollider>();
            if (planeCollider) {
                ResolveSpherePlane(sphere, *planeCollider);
            }
        }
    }

    for (size_t i = 0; i < m_Spheres.size(); ++i) {
        for (size_t j = i + 1; j < m_Spheres.size(); ++j) {
            ResolveSphereSphere(m_Spheres[i], m_Spheres[j]);
        }
    }
}

void SphereDropScenario::OnRender(VkCommandBuffer commandBuffer) {
    auto& material = m_App->GetMaterialSettings();

    // Planes
    for (const auto& plane : m_Planes) {
        PushConstants planePush{};
        planePush.model = plane.body.GetTransform();
        planePush.checkerColorA = material.lightColor;
        planePush.checkerColorB = material.darkColor;
        planePush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

        vkCmdPushConstants(commandBuffer,
            m_App->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &planePush);

        VkBuffer vertexBuffers[] = { plane.buffers.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, plane.buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, plane.buffers.indexCount, 1, 0, 0, 0);
    }

    // Spheres
    for (const auto& sphere : m_Spheres) {
        PushConstants spherePush{};
        spherePush.model = sphere.body.GetTransform();
        spherePush.checkerColorA = material.lightColor;
        spherePush.checkerColorB = material.darkColor;
        spherePush.checkerParams = glm::vec4(material.checkerScale, 0.0f, 0.0f, 0.0f);

        vkCmdPushConstants(commandBuffer,
            m_App->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &spherePush);

        VkBuffer vertexBuffers[] = { sphere.buffers.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, sphere.buffers.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, sphere.buffers.indexCount, 1, 0, 0, 0);
    }
}

void SphereDropScenario::OnUnload() {
    ClearScene();
}

void SphereDropScenario::OnImGui() {
    ImGui::Begin("Sphere Drop Settings");

    ImGui::Text("Physics");
    ImGui::SliderFloat("Gravity", &m_Gravity, -20.0f, 0.0f);
    ImGui::DragFloat("Ground Y", &m_GroundY, 0.1f, -5.0f, 5.0f);
    ImGui::Checkbox("Bounce (Bonus)", &m_UseBounce);
    ImGui::SliderFloat("Plane Friction", &m_PlaneFriction, 0.0f, 1.0f);
    ImGui::SliderFloat("Stop Speed", &m_StopSpeed, 0.0f, 0.5f);

    ImGui::Separator();
    ImGui::Text("Spheres");

    for (size_t i = 0; i < m_Spheres.size(); ++i) {
        auto& sphere = m_Spheres[i];
        std::string label = "Sphere " + std::to_string(i);

        if (ImGui::TreeNode(label.c_str())) {
            glm::vec3 position = sphere.body.GetPosition();
            if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
                sphere.body.SetPosition(position);
            }

            glm::vec3 velocity = sphere.body.GetVelocity();
            if (ImGui::DragFloat3("Velocity", &velocity.x, 0.1f)) {
                sphere.body.SetVelocity(velocity);
            }

            float radius = sphere.body.GetRadius();
            if (ImGui::SliderFloat("Radius", &radius, 0.1f, 2.0f)) {
                sphere.body.SetRadius(radius);
                RebuildSphereMesh(sphere);
            }

            float restitution = sphere.body.GetRestitution();
            if (ImGui::SliderFloat("Restitution", &restitution, 0.0f, 1.0f)) {
                sphere.body.SetRestitution(restitution);
            }

            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Reload Config")) {
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
