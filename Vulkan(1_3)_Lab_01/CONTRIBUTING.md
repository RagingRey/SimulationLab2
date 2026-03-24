# CONTRIBUTING

## Project Requirements

This repository implements a distributed physics simulation for the final lab assessment.

### Mandatory Technical Constraints
- Use **Vulkan** for graphics and **ImGui** for runtime UI.
- Use **Winsock2** for networking.
- Use **glm** as the only math library.
- Implement physics behavior directly in project code (do not use external physics engines).

### Distributed Architecture
- Architecture must be **peer-to-peer**.
- **Client-server** architecture is not allowed.
- Simulated object ownership determines which peer performs simulation and collision response.
- Owners must distribute dynamic state updates to all peers each frame.

### Runtime Concurrency
- Core systems must run asynchronously.
- Graphics, networking, and simulation update rates must be independently configurable at runtime.
- Thread affinity must support the required mapping:
  - Visualisation: core 1
  - Networking: cores 2-3
  - Simulation: core 4+

### Scene/Content Handling
- Scenes are loaded from FlatBuffers schema.
- Missing values must be assigned safe defaults.
- Scene switching is a global UI action and must sync across peers.
- Camera/view mode switching is local UI and must remain local.

### Code Change Expectations
- Preserve deterministic behavior where possible.
- Prefer incremental, testable changes with clear ownership boundaries (simulation, networking, rendering).
- Keep UI controls explicit for demonstration of required features.