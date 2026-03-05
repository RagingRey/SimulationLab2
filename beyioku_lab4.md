# Simulation Lab Book

## Week 4 - Lab 4

March 2nd, 2026

### Q1. Reflect upon the implications of how you manage position

**Question:**
Add tests to collide a moving ball (sphere collider) with a fixed object and assign the resultant velocity using the correct impulse over a fixed timestep.  
Fixed object can be another sphere or a plane.  
Determine the collision normal at contact.

**Solution:**
I implemented a dedicated collision scenario (`CollisionScenario`) that runs focused test cases for:
1. Moving sphere vs stationary sphere  
2. Moving sphere vs axis-aligned plane  
3. Moving sphere vs tilted non-axis-aligned plane  
4. Moving sphere vs skewed non-axis-aligned plane

I detect overlap using colliders, resolve penetration, compute collision normal, and then apply either:
- formative response: stop velocity, or
- bonus response: reflect velocity using restitution.

**Code references (where each part is implemented):**
- Scenario setup and test cases:  
  - `Scenarios/CollisionScenario.cpp` → `SetupTestCase(...)`
- Fixed objects creation (mass = 0):  
  - `Scenarios/CollisionScenario.cpp` → `AddSphere(...)`, `AddPlane(...)`
- Sphere–plane detection/response:  
  - `Scenarios/CollisionScenario.cpp` → `ResolveSpherePlane(...)`
- Sphere–sphere detection/response:  
  - `Scenarios/CollisionScenario.cpp` → `ResolveSphereSphere(...)`
- Collision normal source:  
  - Plane: `SimulationLibrary/Collider.cpp` → `PlaneCollider::DistanceToPoint(...)`
  - Sphere-sphere: normal from center delta in `ResolveSphereSphere(...)`
- Demonstration in sandbox:  
  - `Application/SandboxApplication.cpp` → `RegisterScenario<CollisionScenario>("Collision Tests")`

**Collision response used (bonus reflection):**
`v' = v - (1 + e)(v · n)n`

Where:
- `v` = incoming velocity
- `e` = restitution
- `n` = collision normal

### Q2. Be able to move a ball through space (Summative)

**Question:**
Add tests to collide a ball with another moving ball and assign the correct resultant velocity by applying impulse to each ball over fixed timestep. Start with head-on collisions, then both moving and glancing collisions.

**Solution:**  
I updated sphere-sphere response to impulse-based collision resolution so both bodies receive velocity changes, using equal mass test cases.

**Implemention in C++:**
- Impulse response for two moving spheres: `Scenarios/CollisionScenario.cpp` → `ResolveSphereSphere(...)`
  - Relative velocity: `relativeVelocity = vb - va`
  - Check approach direction: `if (velocityAlongNormal > 0.0f) return;`
  - Impulse scalar:  
    `j = -(1 + e) * dot(rv, n) / (invMassA + invMassB)`
  - Apply to both bodies:
    - `va -= impulse * invA`
    - `vb += impulse * invB`
- Equal-mass test cases: `SetupTestCase(...)`
  - `SphereVsSphereEqualMassOneMoving`
  - `SphereVsSphereEqualMassBothMoving`
  - `SphereVsSphereEqualMassGlancing`

**Test data used:**
1. **Head-on (one moving)**  
   - A: velocity `(2,0,0)`, B: `(0,0,0)`, equal mass
2. **Head-on (both moving)**  
   - A: `(2,0,0)`, B: `(-1,0,0)`, equal mass
3. **Glancing**  
   - Offset centers to force oblique normal and tangential component split

**Reflection:**  
Using impulse on both bodies gives physically correct exchange along the collision normal. In equal-mass head-on cases, velocities swap along the collision axis; in glancing collisions, only normal components are exchanged while tangential components are preserved (with idealized no-friction sphere contact).



### Q3. Be able to make a sphere fall under the effect of gravity (Summative)

**Question:**  
Add tests to collide two moving balls with different masses and compute resultant velocities using impulse over fixed timestep.

**Solution:**  
I reused the same sphere–sphere impulse solver but validated it with different-mass test cases.  
The solver uses inverse masses directly:

- `invA = 1 / m1`
- `invB = 1 / m2`
- `j = -(1 + e) * dot(rv, n) / (invA + invB)`

Then applies impulse to both spheres:
- `vA' = vA - j * invA * n`
- `vB' = vB + j * invB * n`


**Where implemented in C++:**
- Core different-mass impulse logic:  
  `Scenarios/CollisionScenario.cpp` → `ResolveSphereSphere(...)`
- Different-mass test setup cases:  
  `Scenarios/CollisionScenario.cpp` → `SetupTestCase(...)`
  - `SphereVsSphereDifferentMassOneMoving`
  - `SphereVsSphereDifferentMassBothMoving`

**Test data used:**
1. **Different masses, one moving**  
   - `m1 = 1.0`, `u1 = (2,0,0)`  
   - `m2 = 3.0`, `u2 = (0,0,0)`
2. **Different masses, both moving**  
   - `m1 = 1.0`, `u1 = (2,0,0)`  
   - `m2 = 4.0`, `u2 = (-1,0,0)`

**Reflection:**  
Impulse-based response naturally handles different masses through inverse mass weighting.  
In tests, the lighter sphere experiences larger velocity change while the heavier sphere is less affected, which matches expected physical behavior.


### Q4. Be able to detect a collision between spheres and planes (Summative)

**Question:**
Reflect on directly setting post-collision velocities versus calculating collision impulse (and equivalent force over fixed timestep). Implement both approaches and compare consequences when many collisions/forces interact.

**Solution:**  
I implemented and compared two approaches in my collision workflow:

1. **Direct velocity assignment (kinematic response):**  
   After detecting collision, set velocity directly (for example zero or reflected velocity).

2. **Impulse-based response (dynamic response):**  
   Compute impulse magnitude and apply it based on inverse masses:
   - `j = -(1 + e) * dot(rv, n) / (invMassA + invMassB)`
   - `vA' = vA - j * invMassA * n`
   - `vB' = vB + j * invMassB * n`

Given fixed timestep `dt`, impulse can be related to force as:
- `J = F * dt`
- `F = J / dt`

This shows that impulse and force-over-timestep are equivalent representations of momentum change.

**Where implemented in C++:**
- Direct set/reflection style response:
  - `Scenarios/CollisionScenario.cpp` → `ResolveSpherePlane(...)`
  - `Scenarios/SphereDropScenario.cpp` → `ResolveSpherePlane(...)`
- Impulse-based response:
  - `Scenarios/CollisionScenario.cpp` → `ResolveSphereSphere(...)`
- Mass/inverse mass used by solver:
  - `SimulationLibrary/PhysicsObject.cpp` → `SetMass(...)`

**Test observations:**
- Direct velocity assignment is simple and predictable for single contacts.
- Impulse-based response gives physically consistent results for moving bodies with equal or different masses.
- In scenes with many simultaneous contacts and forces (gravity + friction + collisions), impulse formulation scales better and avoids ad-hoc velocity overrides.

**Reflection:**  
Directly setting velocities is useful for quick formative checks, but it bypasses momentum logic and can become inconsistent when multiple interactions occur in one step.  
Impulse-based response is more robust because it is mass-aware, composable with other forces, and physically interpretable through momentum conservation.




### Q5. Add elasticity to your physics model (Formative)

**Question:**  
Add elasticity so that:
- `e = 1` gives fully elastic behavior (no kinetic energy loss along collision normal),
- `e = 0` gives fully inelastic behavior along the collision normal.

**Solution:**  
Elasticity is implemented using restitution in the impulse solver:
`j = -(1 + e) * dot(rv, n) / (invMassA + invMassB)`

I added dedicated Q5 scenario cases:
- `Q5ElasticityOne` (`e = 1`)
- `Q5ElasticityZero` (`e = 0`)

This allows direct comparison of post-collision behavior under the same initial conditions.

**Where implemented in C++ (pinpointed):**
- Elasticity in impulse equation:  
  `Scenarios/CollisionScenario.cpp` → `ResolveSphereSphere(...)`
  - `const float restitution = m_UseBounce ? m_BounceRestitution : 0.0f;`
- Elasticity test cases:  
  `Scenarios/CollisionScenario.cpp` → `SetupTestCase(...)`
  - `Q5ElasticityOne`
  - `Q5ElasticityZero`
- UI control for elasticity:  
  `Scenarios/CollisionScenario.cpp` → `OnImGui()`
  - `ImGui::SliderFloat("Elasticity (e)", &m_BounceRestitution, 0.0f, 1.0f);`

**Test data:**
- Same-mass head-on setup for both tests:
  - Ball A velocity `(2,0,0)`, Ball B velocity `(0,0,0)`
- Run once with `e = 1`, then with `e = 0`.

**Reflection:**  
Elasticity directly controls how much normal relative velocity is preserved after collision.  
`e = 1` preserves bounce strongly; `e = 0` removes normal rebound component.  
Using impulse + elasticity is cleaner than hard-setting velocities because it remains mass-aware and scales better when many collisions happen in one frame.

---