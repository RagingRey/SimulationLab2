# Simulation Lab Book

## Week 2 - Lab 2

February 23, 2026

### Q1. Reflect upon the implications of how you manage position

**Question:**
Be able to load and unload different scenarios easily. Provide an ImGui Main Menu Bar with a Scenario menu to switch active scenarios. Allow scenarios to add their own main-menu UI (e.g., Where could you store the position of your physics objects and what implications would that have on the rest of your system?

What options did you consider?
What were the advantages and disadvantages of each?
What was your final decision and why?colour picker to change background). OnLoad and OnUnload should be virtual in the base Scenario class.

**Reflection:**
**Options considered**
1. **Store position on `Collider`**
   - **Pros:** Collision queries are straightforward because spatial data lives with the collision shape.
   - **Cons:** Rendering and physics now depend on collider state; shape changes become tightly coupled to transforms.

2) **Store position on `PhysicsObject` (transform) and sync the collider**
   - **Pros:** Single source of truth for position/rotation; physics, rendering, and UI stay consistent. Colliders remain lightweight and reusable.
   - **Cons:** Requires syncing the collider after transform changes.

3) **Store position in a separate `Transform` component shared by all systems**
   - **Pros:** Clean separation and extensible design.
   - **Cons:** Extra plumbing and classes for a small project.

**Final decision:**
I stored position in `PhysicsObject` and sync the collider from it. This keeps transforms consistent across physics and rendering, supports multiple collider types, and matches the “PhysicsObject has a Collider” design.
---

### Q2. Be able to move a ball through space (Summative)

**Question:**
You should render your scene 60 times a second. Create an update method that advances the simulation using a `float seconds` timestep. The simulation should be able to run multiple fixed updates between render calls (e.g., 60 fps with 4 updates gives a timestep of 0.004s).

**Solution:**
The app uses a fixed‑timestep accumulator in `SandboxApplication::mainLoop` to call `Scenario::OnUpdate(float seconds)` multiple times between renders. 
Each physics object integrates velocity and position using a selectable integration method.
```c++
void SandboxApplication::mainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        ProcessInput(deltaTime);

        if (m_CurrentScenario) {
            if (!m_IsPaused) {
                m_AccumulatedTime += deltaTime * m_SimulationSpeed;
                while (m_AccumulatedTime >= m_TimeStep) {
                    m_CurrentScenario->OnUpdate(m_TimeStep);
                    m_AccumulatedTime -= m_TimeStep;
                }
            }
            else if (m_StepRequested) {
                m_CurrentScenario->OnUpdate(m_TimeStep);
                m_StepRequested = false;
            }
        }

        drawFrame();
    }
    vkDeviceWaitIdle(m_Device);
}


void PhysicsObject::Update(float deltaTime, float gravity, IntegrationMethod method) {
    glm::vec3 position = GetPosition();

    switch (method) {
        case IntegrationMethod::ExplicitEuler:
            position += m_Velocity * deltaTime;
            m_Velocity.y += gravity * deltaTime;
            break;

        case IntegrationMethod::SemiImplicitEuler:
        default:
            m_Velocity.y += gravity * deltaTime;
            position += m_Velocity * deltaTime;
            break;
    }

    SetPosition(position);
}


//ImGuiLayer
 const char* methods[] = { "Explicit Euler", "Semi-Implicit Euler" };
    int current = static_cast<int>(app->GetIntegrationMethod());
    if (ImGui::Combo("Integrator", &current, methods, IM_ARRAYSIZE(methods))) {
        app->SetIntegrationMethod(static_cast<IntegrationMethod>(current));
    }

    ImGui::Separator();
    ImGui::Text("Stats");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
```

**Test data:**
- Set **Time Step** to `0.004` to get 4 updates per 60 fps frame.
- Set **Gravity** to `0.0` and initial velocity to `(0, 0, -2)` to check `s = ut`.

**Reflection (Euler vs Semi‑Implicit Euler):**
- **Explicit Euler** updates position *before* velocity. It is simple but can drift and become unstable with larger timesteps.
- **Semi‑Implicit Euler** updates velocity *before* position. This is more stable and conserves energy better for physics updates.
- In my scene, Semi‑Implicit Euler keeps the sphere motion more stable at larger timesteps, while Explicit Euler can overshoot slightly.



### Q3. Be able to make a sphere fall under the effect of gravity (Summative)

**Question:**
As part of your physics loop you should accumulate forces acting on each physics object. Store mass and inverseMass in `PhysicsObject`. Apply gravity as a force and integrate motion over time.

**Solution:**
Each physics object stores **mass** and **inverseMass**, and accumulates forces in a per‑frame force buffer. Gravity is applied as `F = m * g`, then acceleration is computed as `a = ΣF / m` before integration.

```c++
void PhysicsObject::Update(float deltaTime, float gravity, IntegrationMethod method) {
    if (m_InverseMass == 0.0f) {
        ClearForces();
        return;
    }

    AddForce({ 0.0f, gravity * m_Mass, 0.0f });

    glm::vec3 position = GetPosition();
    glm::vec3 acceleration = m_ForceAccumulator * m_InverseMass;

    switch (method) {
        case IntegrationMethod::ExplicitEuler:
            position += m_Velocity * deltaTime;
            m_Velocity += acceleration * deltaTime;
            break;

        case IntegrationMethod::SemiImplicitEuler:
        default:
            m_Velocity += acceleration * deltaTime;
            position += m_Velocity * deltaTime;
            break;
    }

    SetPosition(position);
    ClearForces();
}
```


**Test data:**
- Set **Gravity** to `-9.81`, `Time Step` to `0.004`, and `Mass = 1.0`.
- Start at `y = 5`, `v = 0`. Expected at `t = 1s`:
  - `s = ut + 0.5at^2 = 0 + 0.5 * -9.81 * 1^2 = -4.905`
  - New `y ≈ 0.095` (small error depending on integrator)

**Reflection:**
- Accumulating forces allows multiple forces (gravity, impulses, collisions) to be applied in the same frame.
- Storing **inverseMass** avoids repeated division each update and makes static objects (`inverseMass = 0`) easy.
- **Semi‑Implicit Euler** is more stable under constant acceleration; Explicit Euler drifts faster with larger timesteps.Turn off mic 


### Q4. Be able to detect a collision between spheres and planes (Summative)

**Question:**
Use colliders to detect collisions. If a collision is detected, set the velocity of the object to zero.  
Collide a moving sphere against a stationary sphere and a moving sphere against a plane, including a non‑axis‑aligned plane.

**Solution:**
I use `SphereCollider` and `PlaneCollider` to detect overlap.  
Sphere–plane is checked using the plane distance formula.  
Sphere–sphere uses radius‑sum overlap.  
On collision, velocity is set to zero, with a **bonus option** to reflect velocity instead.

```c++
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
```

**Test data:**
- **Sphere–plane:** drop a sphere onto the tilted plane; it contacts and stops (or bounces if bonus enabled).
- **Sphere–sphere:** moving sphere collides with a stationary sphere (`mass = 0`).
- Confirm the moving sphere stops at contact.

**Reflection:**
- Colliders keep collision logic independent of rendering.
- The distance‑to‑plane test works for any plane orientation.
- The optional bounce uses a reflection response for more realistic energy behavior.

---