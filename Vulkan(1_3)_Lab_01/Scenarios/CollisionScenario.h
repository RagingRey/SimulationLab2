#pragma once
#include "Scenario.h"
#include "../Renderer/MeshGenerator.h"
#include "../Application/SandboxApplication.h"
#include "../SimulationLibrary/PhysicsObject.h"
#include "../SimulationLibrary/Collider.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <string>

class CollisionScenario : public Scenario {
private:
    enum class TestCase {
        SphereVsSphereFixed = 0,
        SphereVsSphereEqualMassOneMoving = 1,
        SphereVsSphereEqualMassBothMoving = 2,
        SphereVsSphereEqualMassGlancing = 3,
        SphereVsSphereDifferentMassOneMoving = 4,
        SphereVsSphereDifferentMassBothMoving = 5,
        SphereVsPlaneAxisAligned = 6,
        SphereVsPlaneTilted = 7,
        SphereVsPlaneTiltedSkewed = 8,
        ElasticityOne = 9,
        ElasticityZero = 10
    };

    struct SphereInstance {
        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::vec3 color{ 1.0f, 0.3f, 0.3f };
    };

    struct PlaneInstance {
        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::quat orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
        glm::vec2 size{ 10.0f, 10.0f };
        glm::vec3 color{ 0.4f, 0.4f, 0.4f };
    };

    std::vector<SphereInstance> m_Spheres;
    std::vector<PlaneInstance> m_Planes;

    TestCase m_TestCase = TestCase::SphereVsPlaneTilted;
    float m_Gravity = 0.0f;

    bool m_UseBounce = false;
    float m_BounceRestitution = 0.8f;

    float m_ElapsedTime = 0.0f;
    float m_TargetTime = 1.0f;
    glm::vec3 m_ExpectedPosition{ 0.0f };
    glm::vec3 m_RecordedPosition{ 0.0f };
    bool m_Recorded = false;
    int m_MovingSphereIndex = 0;
    std::string m_TestDescription;

    void ClearScene();
    void SetupTestCase(TestCase testCase);
    void UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position);

    void AddSphere(const glm::vec3& position, const glm::vec3& velocity, float radius, float mass, const glm::vec3& color);
    void AddPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec2& size, const glm::vec3& color);

    void ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane);
    void ResolveSphereSphere(SphereInstance& a, SphereInstance& b);

public:
    explicit CollisionScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnImGui() override;
    void OnUnload() override;

    std::string GetName() const override { return "Q4 Collision Tests"; }
};