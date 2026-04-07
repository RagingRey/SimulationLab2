#include "SceneLoaderFlatBuffer.h"

#include "Generated/Scene_generated.h"
#include "flatbuffers/verifier.h"

#include <fstream>
#include <sstream>

namespace
{
    glm::vec3 ToVec3(const Simulation::Vec3* v, const glm::vec3& fallback = glm::vec3(0.0f))
    {
        if (!v) return fallback;
        return { v->x(), v->y(), v->z() };
    }

    SimRuntime::RotationEuler ToEuler(const Simulation::RotationEuler* e)
    {
        if (!e) return {};
        return { e->yaw(), e->pitch(), e->roll() };
    }

    SimRuntime::Transform ToTransform(const Simulation::Transform* t)
    {
        SimRuntime::Transform out{};
        if (!t) return out;

        out.position = ToVec3(&t->position());
        out.orientation = ToEuler(&t->orientation());

        const glm::vec3 scale = ToVec3(&t->scale(), glm::vec3(1.0f));
        out.scale = (scale.x == 0.0f && scale.y == 0.0f && scale.z == 0.0f)
            ? glm::vec3(1.0f)
            : scale;
        return out;
    }

    SimRuntime::OwnerType ToOwner(Simulation::ObjectOwnerType owner)
    {
        switch (owner)
        {
        case Simulation::ObjectOwnerType::ObjectOwnerType_ONE: return SimRuntime::OwnerType::One;
        case Simulation::ObjectOwnerType::ObjectOwnerType_TWO: return SimRuntime::OwnerType::Two;
        case Simulation::ObjectOwnerType::ObjectOwnerType_THREE: return SimRuntime::OwnerType::Three;
        case Simulation::ObjectOwnerType::ObjectOwnerType_FOUR: return SimRuntime::OwnerType::Four;
        default: return SimRuntime::OwnerType::One;
        }
    }

    SimRuntime::CollisionType ToCollision(Simulation::CollisionType c)
    {
        return (c == Simulation::CollisionType::CollisionType_CONTAINER)
            ? SimRuntime::CollisionType::Container
            : SimRuntime::CollisionType::Solid;
    }

    std::string ReadAllBytes(const std::string& path, std::vector<uint8_t>& bytes)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return "Failed to open scene file: " + path;
        }

        const std::streamsize size = file.tellg();
        if (size <= 0)
        {
            return "Scene file is empty: " + path;
        }

        file.seekg(0, std::ios::beg);
        bytes.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
        {
            return "Failed to read scene file: " + path;
        }

        return {};
    }

    SimRuntime::SpawnerOwnerType ToSpawnerOwner(Simulation::SpawnerOwnerType owner)
    {
        switch (owner)
        {
        case Simulation::SpawnerOwnerType_ONE: return SimRuntime::SpawnerOwnerType::One;
        case Simulation::SpawnerOwnerType_TWO: return SimRuntime::SpawnerOwnerType::Two;
        case Simulation::SpawnerOwnerType_THREE: return SimRuntime::SpawnerOwnerType::Three;
        case Simulation::SpawnerOwnerType_FOUR: return SimRuntime::SpawnerOwnerType::Four;
        case Simulation::SpawnerOwnerType_SEQUENTIAL: return SimRuntime::SpawnerOwnerType::Sequential;
        default: return SimRuntime::SpawnerOwnerType::One;
        }
    }

    SimRuntime::FloatRangeDef ToFloatRange(const Simulation::FloatRange* range, float fallbackMin, float fallbackMax)
    {
        SimRuntime::FloatRangeDef out{};
        if (!range) {
            out.min = fallbackMin;
            out.max = fallbackMax;
            return out;
        }

        out.min = range->min();
        out.max = range->max();
        if (out.max < out.min) {
            const float t = out.min;
            out.min = out.max;
            out.max = t;
        }
        return out;
    }

    SimRuntime::Vec3RangeDef ToVec3Range(const Simulation::Vec3Range* range, const glm::vec3& fallbackMin = glm::vec3(0.0f), const glm::vec3& fallbackMax = glm::vec3(0.0f))
    {
        SimRuntime::Vec3RangeDef out{};
        if (!range) {
            out.min = fallbackMin;
            out.max = fallbackMax;
            return out;
        }

        out.min = ToVec3(&range->min(), fallbackMin);
        out.max = ToVec3(&range->max(), fallbackMax);
        return out;
    }
}

SimRuntime::SceneLoadResult SimRuntime::SceneLoaderFlatBuffer::LoadFromFile(const std::string& filePath)
{
    SceneLoadResult result{};

    std::vector<uint8_t> bytes;
    if (const std::string err = ReadAllBytes(filePath, bytes); !err.empty())
    {
        result.error = err;
        return result;
    }

    flatbuffers::Verifier verifier(bytes.data(), bytes.size());
    if (!Simulation::VerifySceneBuffer(verifier))
    {
        result.error = "FlatBuffer verification failed for scene: " + filePath;
        return result;
    }

    const Simulation::Scene* fbsScene = Simulation::GetScene(bytes.data());
    if (!fbsScene)
    {
        result.error = "Failed to parse root Scene object.";
        return result;
    }

    result.scene.name = (fbsScene->name() && !fbsScene->name()->str().empty())
        ? fbsScene->name()->str()
        : "Unnamed Scene";

    result.scene.description = fbsScene->description() ? fbsScene->description()->str() : "";
    result.scene.gravityOn = fbsScene->gravity_on();

    // Cameras
    if (const auto* cams = fbsScene->cameras())
    {
        for (size_t i = 0; i < cams->size(); ++i)
        {
            const auto* c = cams->Get(i);
            if (!c) continue;

            CameraDef cam{};
            cam.name = (c->name() && !c->name()->str().empty()) ? c->name()->str() : ("camera " + std::to_string(i + 1));
            cam.transform = ToTransform(c->transform());

            switch (c->camera_type_type())
            {
            case Simulation::CameraType::CameraType_PerspectiveCamera:
            {
                cam.projection = CameraProjection::Perspective;
                const auto* p = c->camera_type_as_PerspectiveCamera();
                if (p)
                {
                    cam.fov = p->fov();
                    cam.nearPlane = p->near();
                    cam.farPlane = p->far();
                }
                break;
            }
            case Simulation::CameraType::CameraType_OrthographicCamera:
            {
                cam.projection = CameraProjection::Orthographic;
                const auto* o = c->camera_type_as_OrthographicCamera();
                if (o)
                {
                    cam.orthoSize = o->size();
                    cam.nearPlane = o->near();
                    cam.farPlane = o->far();
                }
                break;
            }
            default:
                result.warnings.push_back("Camera '" + cam.name + "' missing camera_type. Defaulting to perspective.");
                break;
            }

            result.scene.cameras.push_back(cam);
        }
    }

    // Materials
    if (const auto* mats = fbsScene->materials())
    {
        for (size_t i = 0; i < mats->size(); ++i)
        {
            const auto* m = mats->Get(i);
            if (!m) continue;

            MaterialDef mat{};
            mat.name = (m->name() && !m->name()->str().empty()) ? m->name()->str() : ("material " + std::to_string(i + 1));
            mat.density = (m->density() > 0.0f) ? m->density() : 1000.0f;
            result.scene.materials.push_back(mat);
        }
    }

    // Interactions
    if (const auto* interactions = fbsScene->interactions())
    {
        for (size_t i = 0; i < interactions->size(); ++i)
        {
            const auto* x = interactions->Get(i);
            if (!x) continue;

            MaterialInteractionDef def{};
            def.materialA = x->material_a() ? x->material_a()->str() : "default";
            def.materialB = x->material_b() ? x->material_b()->str() : "default";
            def.restitution = x->restitution();
            def.staticFriction = x->static_friction();
            def.dynamicFriction = x->dynamic_friction();

            result.scene.interactions.push_back(def);
        }
    }

    // Objects
    if (const auto* objs = fbsScene->objects())
    {
        size_t unnamedCounter = 1;
        for (size_t i = 0; i < objs->size(); ++i)
        {
            const auto* o = objs->Get(i);
            if (!o) continue;

            ObjectDef obj{};
            obj.name = (o->name() && !o->name()->str().empty())
                ? o->name()->str()
                : ("object " + std::to_string(unnamedCounter++));
            obj.transform = ToTransform(o->transform());
            obj.material = (o->material() && !o->material()->str().empty()) ? o->material()->str() : "default";
            obj.collisionType = ToCollision(o->collision_type());

            // Shape
            switch (o->shape_type())
            {
            case Simulation::Shape::Shape_Sphere:
                obj.shapeType = ShapeType::Sphere;
                if (const auto* s = o->shape_as_Sphere()) obj.radius = s->radius();
                break;
            case Simulation::Shape::Shape_Plane:
                obj.shapeType = ShapeType::Plane;
                if (const auto* p = o->shape_as_Plane()) obj.planeNormal = ToVec3(p->normal(), glm::vec3(0.0f, 1.0f, 0.0f));
                break;
            case Simulation::Shape::Shape_Capsule:
                obj.shapeType = ShapeType::Capsule;
                if (const auto* c = o->shape_as_Capsule()) { obj.radius = c->radius(); obj.height = c->height(); }
                break;
            case Simulation::Shape::Shape_Cylinder:
                obj.shapeType = ShapeType::Cylinder;
                if (const auto* c = o->shape_as_Cylinder()) { obj.radius = c->radius(); obj.height = c->height(); }
                break;
            case Simulation::Shape::Shape_Cuboid:
                obj.shapeType = ShapeType::Cuboid;
                if (const auto* c = o->shape_as_Cuboid()) obj.size = ToVec3(c->size(), glm::vec3(1.0f));
                break;
            default:
                result.warnings.push_back("Object '" + obj.name + "' missing shape. Defaulting to Sphere(radius=0.5).");
                break;
            }

            // Behaviour
            switch (o->behaviour_type())
            {
            case Simulation::Behaviour::Behaviour_StaticObject:
                obj.behaviourType = BehaviourType::Static;
                break;
            case Simulation::Behaviour::Behaviour_SimulatedObject:
            {
                obj.behaviourType = BehaviourType::Simulated;
                if (const auto* sim = o->behaviour_as_SimulatedObject())
                {
                    if (const auto* state = sim->initial_state())
                    {
                        obj.initialState.linearVelocity = ToVec3(&state->linear_velocity());
                        obj.initialState.angularVelocityDeg = ToVec3(&state->angular_velocity());
                    }
                    obj.owner = ToOwner(sim->owner());
                }
                break;
            }
            case Simulation::Behaviour::Behaviour_AnimatedObject:
            {
                obj.behaviourType = BehaviourType::Animated;
                if (const auto* anim = o->behaviour_as_AnimatedObject())
                {
                    obj.totalDuration = anim->total_duration();
                    obj.easing = (anim->easing() == Simulation::EasingType::EasingType_SMOOTHSTEP)
                        ? EasingType::SmoothStep : EasingType::Linear;

                    switch (anim->path_mode())
                    {
                    case Simulation::PathMode::PathMode_LOOP: obj.pathMode = PathMode::Loop; break;
                    case Simulation::PathMode::PathMode_REVERSE: obj.pathMode = PathMode::Reverse; break;
                    default: obj.pathMode = PathMode::Stop; break;
                    }

                    if (const auto* wps = anim->waypoints())
                    {
                        for (size_t wi = 0; wi < wps->size(); ++wi)
                        {
                            const auto* w = wps->Get(wi);
                            if (!w) continue;

                            Waypoint wp{};
                            wp.position = ToVec3(w->position());
                            wp.rotation = ToEuler(w->rotation());
                            wp.time = w->time();
                            obj.waypoints.push_back(wp);
                        }
                    }
                }
                break;
            }
            default:
                result.warnings.push_back("Object '" + obj.name + "' missing behaviour. Defaulting to StaticObject.");
                obj.behaviourType = BehaviourType::Static;
                break;
            }

            result.scene.objects.push_back(obj);
        }
    }

    // Spawners
    const auto* spawnerTypes = fbsScene->spawners_type();
    const auto* spawnerValues = fbsScene->spawners();

    if (spawnerTypes && spawnerValues)
    {
        const auto count = std::min(spawnerTypes->size(), spawnerValues->size());

        if (spawnerTypes->size() != spawnerValues->size()) {
            result.warnings.push_back("Spawner union vectors have mismatched sizes. Parsing common prefix only.");
        }

        auto parseBase = [&](const Simulation::BaseSpawner* base, SpawnerDef& dst)
        {
            if (!base) {
                result.warnings.push_back("Spawner missing base. Defaults applied.");
                return;
            }

            dst.base.name = (base->name() && !base->name()->str().empty()) ? base->name()->str() : "spawner";
            dst.base.startTime = base->start_time();
            dst.base.material = (base->material() && !base->material()->str().empty()) ? base->material()->str() : "default";
            dst.base.owner = ToSpawnerOwner(base->owner());
            dst.base.linearVelocity = ToVec3Range(base->linear_velocity());
            dst.base.angularVelocity = ToVec3Range(base->angular_velocity());

            switch (base->spawn_type_type())
            {
            case Simulation::SpawnType_SingleBurstSpawn:
                dst.base.spawnType.mode = SpawnMode::SingleBurst;
                if (const auto* sb = base->spawn_type_as_SingleBurstSpawn()) {
                    dst.base.spawnType.count = std::max(1u, sb->count());
                }
                break;
            case Simulation::SpawnType_RepeatingSpawn:
                dst.base.spawnType.mode = SpawnMode::Repeating;
                if (const auto* rp = base->spawn_type_as_RepeatingSpawn()) {
                    dst.base.spawnType.interval = (rp->interval() > 0.0f) ? rp->interval() : 1.0f;
                    dst.base.spawnType.maxCount = std::max(1u, rp->max_count());
                }
                break;
            default:
                result.warnings.push_back("Spawner '" + dst.base.name + "' missing spawn_type. Defaulting to SingleBurst.");
                break;
            }

            switch (base->location_type())
            {
            case Simulation::SpawnLocation_FixedLocation:
                dst.base.location.type = SpawnLocationType::Fixed;
                if (const auto* fx = base->location_as_FixedLocation()) {
                    dst.base.location.fixedTransform = ToTransform(fx->transform());
                }
                break;
            case Simulation::SpawnLocation_RandomBox:
                dst.base.location.type = SpawnLocationType::RandomBox;
                if (const auto* rb = base->location_as_RandomBox()) {
                    dst.base.location.randomBoxMin = ToVec3(rb->min(), glm::vec3(0.0f));
                    dst.base.location.randomBoxMax = ToVec3(rb->max(), glm::vec3(0.0f));
                }
                break;
            case Simulation::SpawnLocation_RandomSphere:
                dst.base.location.type = SpawnLocationType::RandomSphere;
                if (const auto* rs = base->location_as_RandomSphere()) {
                    dst.base.location.randomSphereCenter = ToVec3(rs->center(), glm::vec3(0.0f));
                    dst.base.location.randomSphereRadius = (rs->radius() > 0.0f) ? rs->radius() : 1.0f;
                }
                break;
            default:
                result.warnings.push_back("Spawner '" + dst.base.name + "' missing location. Defaulting to FixedLocation.");
                break;
            }
        };

        for (flatbuffers::uoffset_t i = 0; i < count; ++i)
        {
            SpawnerDef spawner{};
            const auto type = spawnerTypes->GetEnum<Simulation::SpawnerType>(i);
            const void* raw = spawnerValues->Get(i);

            switch (type)
            {
            case Simulation::SpawnerType_SphereSpawner:
            {
                spawner.shape = SpawnerShapeType::Sphere;
                const auto* s = static_cast<const Simulation::SphereSpawner*>(raw);
                if (s) {
                    parseBase(s->base(), spawner);
                    spawner.radiusRange = ToFloatRange(s->radius_range(), 0.5f, 0.5f);
                }
                break;
            }
            case Simulation::SpawnerType_CylinderSpawner:
            {
                spawner.shape = SpawnerShapeType::Cylinder;
                const auto* s = static_cast<const Simulation::CylinderSpawner*>(raw);
                if (s) {
                    parseBase(s->base(), spawner);
                    spawner.radiusRange = ToFloatRange(s->radius_range(), 0.5f, 0.5f);
                    spawner.heightRange = ToFloatRange(s->height_range(), 1.0f, 1.0f);
                }
                break;
            }
            case Simulation::SpawnerType_CapsuleSpawner:
            {
                spawner.shape = SpawnerShapeType::Capsule;
                const auto* s = static_cast<const Simulation::CapsuleSpawner*>(raw);
                if (s) {
                    parseBase(s->base(), spawner);
                    spawner.radiusRange = ToFloatRange(s->radius_range(), 0.5f, 0.5f);
                    spawner.heightRange = ToFloatRange(s->height_range(), 1.0f, 1.0f);
                }
                break;
            }
            case Simulation::SpawnerType_CuboidSpawner:
            {
                spawner.shape = SpawnerShapeType::Cuboid;
                const auto* s = static_cast<const Simulation::CuboidSpawner*>(raw);
                if (s) {
                    parseBase(s->base(), spawner);
                    spawner.sizeRange = ToVec3Range(s->size_range(), glm::vec3(1.0f), glm::vec3(1.0f));
                }
                break;
            }
            default:
                result.warnings.push_back("Unknown spawner type encountered. Entry skipped.");
                continue;
            }

            result.scene.spawners.push_back(std::move(spawner));
        }
    }

    result.success = true;
    return result;
}