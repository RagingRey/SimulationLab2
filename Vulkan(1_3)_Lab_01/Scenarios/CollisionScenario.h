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
        ElasticityZero = 10,

        SphereVsCuboidAxisAligned = 11,
        SphereVsCuboidRotated = 12,
        CuboidVsCuboidAxisAligned = 13,
        CuboidVsPlaneTilted = 14
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

    struct BoxInstance {
        PhysicsObject body;
        Mesh mesh;
        SandboxApplication::MeshBuffers buffers{};
        glm::vec3 halfExtents{ 0.5f, 0.5f, 0.5f };
        glm::vec3 color{ 0.3f, 0.8f, 1.0f };
    };

    std::vector<SphereInstance> m_Spheres;
    std::vector<PlaneInstance> m_Planes;
    std::vector<BoxInstance> m_Boxes;

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

    int m_OverlapCount = 0;

    void ClearScene();
    void SetupTestCase(TestCase testCase);
    void UpdatePlaneTransform(PlaneInstance& plane, const glm::vec3& position);

    void AddSphere(const glm::vec3& position, const glm::vec3& velocity, float radius, float mass, const glm::vec3& color);
    void AddPlane(const glm::vec3& point, const glm::vec3& normal, const glm::vec2& size, const glm::vec3& color);
    void AddBox(const glm::vec3& position, const glm::quat& orientation, const glm::vec3& halfExtents, const glm::vec3& velocity, float mass, const glm::vec3& color);

    void ResolveSpherePlane(SphereInstance& sphere, const PlaneCollider& plane);
    void ResolveSphereSphere(SphereInstance& a, SphereInstance& b);

    void ResolveSphereBox(SphereInstance& sphere, BoxInstance& box);
    void ResolveBoxPlane(BoxInstance& box, const PlaneCollider& plane);
    void ResolveBoxBox(BoxInstance& a, BoxInstance& b);

    void ComputeOverlapCount();

public:
    explicit CollisionScenario(SandboxApplication* app) : Scenario(app) {}

    void OnLoad() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VkCommandBuffer commandBuffer) override;
    void OnImGui() override;
    void OnUnload() override;

    void GetSelectionItems(std::vector<Scenario::SceneSelectionItem>& out);

    std::string GetName() const override { return "Q4 Collision Tests"; }
};