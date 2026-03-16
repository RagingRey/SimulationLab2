#pragma once
#include "Scenario.h"
#include "../Renderer/MeshGenerator.h"
#include "../Application/SandboxApplication.h"
#include "../SimulationLibrary/PhysicsObject.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

class OrientationScenario : public Scenario {
private:
    struct RotationTest {
        std::string name;
        glm::vec3 degrees;
    };

    struct AngularVelocityTest {
        std::string name;
        glm::vec3 degreesPerSecond;
        float durationSeconds = 1.0f;
    };

    PhysicsObject m_Object{};
    PhysicsObject m_Ground{};

    Mesh m_ObjectMesh;
    Mesh m_GroundMesh;
    SandboxApplication::MeshBuffers m_ObjectBuffers{};
    SandboxApplication::MeshBuffers m_GroundBuffers{};

    // Q1
    glm::vec3 m_AngularDisplacementDegrees{ 0.0f, 90.0f, 0.0f };
    std::vector<RotationTest> m_Tests;
    int m_SelectedTest = 0;

    // Q2
    bool m_AutoRotate = false;
    glm::vec3 m_AngularVelocityDegreesPerSecond{ 0.0f, 90.0f, 0.0f };
    float m_TestDurationSeconds = 1.0f;

    std::vector<AngularVelocityTest> m_AngularVelocityTests;
    int m_SelectedAngularVelocityTest = 0;
    std::string m_LastAngularVelocityResult = "Not run";

    void BuildTests();
    void BuildAngularVelocityTests();
    void ApplyDisplacementDegrees(const glm::vec3& degrees);
    bool IsCardinal(const glm::vec3& axis, float tolerance = 0.05f) const;
    void RunAngularVelocityTest(const AngularVelocityTest& test);

public:
    explicit OrientationScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnImGui() override;
    void OnUnload() override;
    std::string GetName() const override { return "Orientation Tests"; }
};