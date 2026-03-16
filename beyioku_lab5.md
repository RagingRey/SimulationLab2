# Simulation Lab Book

## Week 5 - Lab 5

March 15nd, 2026

### Q1. Add orientation and angular displacement 

**Question:**  
Add tests to apply angular displacement to physics objects.  
Test values include 90° (`π/2`), 180° (`π`), 270° (`3π/2`), 360° (`2π`) rotations in x, y, z, and combinations.  
Document tests in a markdown table and demonstrate in sandbox.

**Solution:**  
I extended `PhysicsObject` with angular displacement support:
- `SetOrientationEuler(...)`
- `ApplyAngularDisplacementEuler(...)`

I then added a dedicated sandbox scenario, `OrientationScenario`, to apply and visualize rotations using manual input, preset test cases, and auto-rotation.

**Where implemented in C++ (pinpointed):**
- Angular displacement API:
  - `SimulationLibrary/PhysicsObject.h` → `ApplyAngularDisplacementEuler(...)`
- Angular displacement implementation:
  - `SimulationLibrary/PhysicsObject.cpp` → `ApplyAngularDisplacementEuler(...)`
- Sandbox demonstration:
  - `Scenarios/OrientationScenario.cpp`
    - `BuildTests()`
    - `OnImGui()` (run preset tests and display transformed basis vectors)
    - `OnUpdate()` (continuous angular displacement)
- Scenario registration:
  - `Application/SandboxApplication.cpp` → `RegisterScenario<OrientationScenario>("Orientation Tests")`

**Test Table**

| Test | Input Degrees (X,Y,Z) | Radians (X,Y,Z) | Expected | Observed | Pass |
|---|---:|---:|---|---|---|
| X 90 | (90,0,0) | (1.571,0,0) | Basis rotates about X | Basis vectors update correctly | ✅ |
| X 180 | (180,0,0) | (3.142,0,0) | Inverted Y/Z directions | Observed inversion | ✅ |
| X 270 | (270,0,0) | (4.712,0,0) | 3/4 turn around X | Correct orientation | ✅ |
| X 360 | (360,0,0) | (6.283,0,0) | Returns to initial orientation | Returned to initial basis | ✅ |
| Y 90 | (0,90,0) | (0,1.571,0) | Basis rotates about Y | Correct | ✅ |
| Y 180 | (0,180,0) | (0,3.142,0) | Front reverses | Correct | ✅ |
| Y 270 | (0,270,0) | (0,4.712,0) | 3/4 turn around Y | Correct | ✅ |
| Y 360 | (0,360,0) | (0,6.283,0) | Returns to initial orientation | Correct | ✅ |
| Z 90 | (0,0,90) | (0,0,1.571) | Basis rotates about Z | Correct | ✅ |
| Z 180 | (0,0,180) | (0,0,3.142) | Right/Up invert | Correct | ✅ |
| Z 270 | (0,0,270) | (0,0,4.712) | 3/4 turn around Z | Correct | ✅ |
| Z 360 | (0,0,360) | (0,0,6.283) | Returns to initial orientation | Correct | ✅ |
| Combo XYZ | (45,30,60) | (0.785,0.524,1.047) | Combined orientation | Correct combined basis change | ✅ |

**Reflection:**  
Representing orientation with quaternions avoids gimbal-lock issues from incremental Euler edits while still allowing Euler input for readable tests.  
Applying angular displacement via quaternion composition gave stable and repeatable rotation behavior across all required test angles.


### Q2. Add angular velocity (Summative)

**Question:**  
Add tests to apply angular velocity (radians/second) to a physics object over fixed timesteps, run for specific durations (1, 2, 3, 4 seconds), and verify cardinal axes remain correct.

**Solution:**  
I added angular velocity support directly to `PhysicsObject` as a radians/second vector and integrated it by timestep:

- `SetAngularVelocity(...)`
- `IntegrateAngularVelocity(deltaTime)`
- `ApplyAngularDisplacementEuler(angularVelocity * deltaTime)`

I then extended `OrientationScenario` with preset angular-velocity tests and duration-based simulation.

**Where implemented in C++ (pinpointed):**
- Angular velocity API:
  - `SimulationLibrary/PhysicsObject.h`
    - `GetAngularVelocity()`
    - `SetAngularVelocity(...)`
    - `IntegrateAngularVelocity(float)`
- Angular velocity integration:
  - `SimulationLibrary/PhysicsObject.cpp`
    - `IntegrateAngularVelocity(float)`
- Scenario tests/UI:
  - `Scenarios/OrientationScenario.cpp`
    - `BuildAngularVelocityTests()`
    - `RunAngularVelocityTest(...)`
    - `OnUpdate(...)`
    - `OnImGui(...)`

**Test Table**

| Test | Angular Velocity (deg/s) | Duration (s) | Expected | Observed | Pass |
|---|---:|---:|---|---|---|
| X-90 for 1s | (90,0,0) | 1 | 90° about X | Basis rotated correctly | ✅ |
| X-90 for 2s | (90,0,0) | 2 | 180° about X | Basis rotated correctly | ✅ |
| X-90 for 3s | (90,0,0) | 3 | 270° about X | Basis rotated correctly | ✅ |
| X-90 for 4s | (90,0,0) | 4 | 360° returns to start | Basis returned to initial | ✅ |
| Y-180 for 1s | (0,180,0) | 1 | 180° about Y | Basis rotated correctly | ✅ |
| Y-270 for 1s | (0,270,0) | 1 | 270° about Y | Basis rotated correctly | ✅ |
| Z-360 for 1s | (0,0,360) | 1 | 360° about Z | Returned to initial | ✅ |
| Combo for 1s | (90,180,270) | 1 | Combined rotation | Cardinal basis remained valid | ✅ |

**Reflection:**  
Using angular velocity in radians/second and integrating by fixed timestep made rotation deterministic and consistent with linear integration style.  
Duration-based tests (1–4s) made it easy to validate expected orientation cycles and confirm that cardinal axis behavior remains stable.



### Q3.  Reflect on the Different Approaches on Storing Orientation

**Question:**  
How have you stored your orientation? What other options could you choose? What are the advantages and disadvantages of each approach?

**Answer / Reflection:**

In my current implementation, orientation is stored as part of the object's transform matrix (`m_Transform`) inside `PhysicsObject`, and converted to/from quaternions when needed:
- `SimulationLibrary/PhysicsObject.h` / `.cpp`
  - `GetOrientation()` uses `glm::quat_cast(m_Transform)`
  - `SetOrientation(...)` writes rotation back into `m_Transform`
  - `SetOrientationEuler(...)` and `ApplyAngularDisplacementEuler(...)` apply angular changes

So the **single source of truth** is the transform matrix, while quaternion/Euler are used as interfaces for update logic.

---

**Other orientation storage options considered**

1. **Store Euler angles (`pitch, yaw, roll`)**
   - **Advantages:** Easy to read/debug in UI; intuitive for manual editing.
   - **Disadvantages:** Vulnerable to gimbal lock; axis order issues; less stable for repeated integration.

2. **Store Quaternion directly**
   - **Advantages:** Compact, stable for interpolation/integration, avoids gimbal lock.
   - **Disadvantages:** Less intuitive to inspect manually; must be normalized; converting for UI is less convenient.

3. **Store 3x3 Rotation Matrix**
   - **Advantages:** Direct basis vectors (right/up/forward), efficient for transforming vectors, no quaternion conversion needed.
   - **Disadvantages:** Can drift from orthonormal form over many updates; requires re-orthonormalization.

4. **Store Axis-Angle**
   - **Advantages:** Good for representing one clear rotation around an axis.
   - **Disadvantages:** Less convenient for chaining many rotations and general simulation updates.

---

**Final decision and reason**

I kept orientation embedded in the transform matrix and used quaternion-based operations for angular displacement/velocity updates.  
This gave a practical balance:
- matrix-based rendering integration (already required by Vulkan pipeline),
- quaternion stability for rotation updates,
- Euler-friendly input for test setup and UI controls.