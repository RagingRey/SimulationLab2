#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace SimRuntime
{
    enum class OwnerType
    {
        One = 0,
        Two,
        Three,
        Four
    };

    enum class CollisionType
    {
        Solid = 0,
        Container
    };

    enum class CameraProjection
    {
        Perspective = 0,
        Orthographic
    };

    enum class ShapeType
    {
        Sphere = 0,
        Plane,
        Capsule,
        Cylinder,
        Cuboid
    };

    enum class BehaviourType
    {
        Static = 0,
        Simulated,
        Animated
    };

    enum class EasingType
    {
        Linear = 0,
        SmoothStep
    };

    enum class PathMode
    {
        Stop = 0,
        Loop,
        Reverse
    };

    enum class SpawnerOwnerType
    {
        One = 0,
        Two,
        Three,
        Four,
        Sequential
    };

    enum class SpawnMode
    {
        SingleBurst = 0,
        Repeating
    };

    enum class SpawnLocationType
    {
        Fixed = 0,
        RandomBox,
        RandomSphere
    };

    enum class SpawnerShapeType
    {
        Sphere = 0,
        Cylinder,
        Capsule,
        Cuboid
    };

    struct RotationEuler
    {
        float yaw = 0.0f;
        float pitch = 0.0f;
        float roll = 0.0f;
    };

    struct Transform
    {
        glm::vec3 position{ 0.0f };
        RotationEuler orientation{};
        glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
    };

    struct PhysicsState
    {
        glm::vec3 linearVelocity{ 0.0f };
        glm::vec3 angularVelocityDeg{ 0.0f };
    };

    struct Waypoint
    {
        glm::vec3 position{ 0.0f };
        RotationEuler rotation{};
        float time = 0.0f;
    };

    struct FloatRangeDef
    {
        float min = 0.0f;
        float max = 0.0f;
    };

    struct Vec3RangeDef
    {
        glm::vec3 min{ 0.0f };
        glm::vec3 max{ 0.0f };
    };

    struct CameraDef
    {
        std::string name = "Camera";
        Transform transform{};
        CameraProjection projection = CameraProjection::Perspective;

        float fov = 60.0f;
        float orthoSize = 10.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
    };

    struct MaterialDef
    {
        std::string name = "default";
        float density = 1000.0f;
    };

    struct MaterialInteractionDef
    {
        std::string materialA = "default";
        std::string materialB = "default";
        float restitution = 0.4f;
        float staticFriction = 0.6f;
        float dynamicFriction = 0.5f;
    };

    struct ObjectDef
    {
        std::string name;
        Transform transform{};
        std::string material = "default";

        ShapeType shapeType = ShapeType::Sphere;
        float radius = 0.5f;
        float height = 1.0f;
        glm::vec3 size{ 1.0f, 1.0f, 1.0f };
        glm::vec3 planeNormal{ 0.0f, 1.0f, 0.0f };

        BehaviourType behaviourType = BehaviourType::Static;
        PhysicsState initialState{};
        OwnerType owner = OwnerType::One;

        std::vector<Waypoint> waypoints;
        float totalDuration = 0.0f;
        EasingType easing = EasingType::Linear;
        PathMode pathMode = PathMode::Stop;

        CollisionType collisionType = CollisionType::Solid;
    };

    struct SpawnTypeDef
    {
        SpawnMode mode = SpawnMode::SingleBurst;
        uint32_t count = 1;
        float interval = 1.0f;
        uint32_t maxCount = 1;
    };

    struct SpawnLocationDef
    {
        SpawnLocationType type = SpawnLocationType::Fixed;
        Transform fixedTransform{};
        glm::vec3 randomBoxMin{ 0.0f };
        glm::vec3 randomBoxMax{ 0.0f };
        glm::vec3 randomSphereCenter{ 0.0f };
        float randomSphereRadius = 1.0f;
    };

    struct BaseSpawnerDef
    {
        std::string name = "spawner";
        float startTime = 0.0f;
        SpawnTypeDef spawnType{};
        SpawnLocationDef location{};
        Vec3RangeDef linearVelocity{};
        Vec3RangeDef angularVelocity{};
        std::string material = "default";
        SpawnerOwnerType owner = SpawnerOwnerType::One;
    };

    struct SpawnerDef
    {
        SpawnerShapeType shape = SpawnerShapeType::Sphere;
        BaseSpawnerDef base{};
        FloatRangeDef radiusRange{ 0.5f, 0.5f };
        FloatRangeDef heightRange{ 1.0f, 1.0f };
        Vec3RangeDef sizeRange{ glm::vec3(1.0f), glm::vec3(1.0f) };
    };

    struct SceneRuntime
    {
        std::string name = "Unnamed Scene";
        std::string description = "";
        bool gravityOn = true;

        std::vector<CameraDef> cameras;
        std::vector<ObjectDef> objects;
        std::vector<SpawnerDef> spawners;
        std::vector<MaterialDef> materials;
        std::vector<MaterialInteractionDef> interactions;
    };
}