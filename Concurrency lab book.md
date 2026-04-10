## Final Lab (700120) - Concurrency + Integration Tracking

### Current Feature Status
- FlatBuffers scene loading with defaults: `PARTIALLY DONE`
- Peer-to-peer Winsock2 networking: `PARTIALLY DONE`
- Distributed ownership + replication: `PARTIALLY DONE`
- Async loops (render/network/simulation at independent Hz): `DONE`
- Thread affinity mapping: `DONE`
- Local/global UI split for networked simulator: `PARTIALLY DONE`
- Drift correction / interpolation: `PARTIALLY DONE`
- Advanced Simulation Feature (Flocking and Steering): `DONE`
- Extended Flocking (segmentation + performance comparison): `DONE`

### Implemented So Far (Codebase Snapshot)
- Existing Vulkan render loop and frame sync in `SandboxApplication`: `DONE`
- Existing scenario switching UI in `ImGuiLayer`: `DONE`
- Existing local camera control + camera presets: `DONE`
- Existing fixed-step simulation controls (pause/step/timestep/speed): `DONE`
- Network/distributed systems: `PARTIALLY DONE`

### Evidence / Notes
- Current architecture is single-process, single-node simulation with scenario abstraction.
- UI supports local ownership selection plus replicated global commands in the `FlatBuffer Preview` path.
- `Flocking (Networked)` now demonstrates owner-authoritative boid simulation with replicated remote states and interpolation.
- Next implementation milestone is Level 3 extension for flocking:
  - baseline (no segmentation),
  - uniform grid,
  - octree,
  - benchmark comparison (memory + collision + update costs).

### Progress Log
- 2026-03-31: Added Final Lab tracking section and baseline status.
- 2026-04-01: **PARTIALLY DONE** — FlatBuffers pipeline started.
  - Added runtime scene representation (`SceneRuntime`).
  - Added FlatBuffers scene loader (`SceneLoaderFlatBuffer`) with verification and fallback defaults for missing values.
  - Parsed: scene metadata, cameras, materials, material interactions, objects (shape + behaviour).
  - Pending: spawner parsing, integration into app scenario switching/network replication.
- 2026-04-03: **PARTIALLY DONE** — Added spawner parsing to FlatBuffers loader.
  - Integrated FlatBuffer bootstrap attempt into app startup with graceful fallback to existing scenarios if no provided scene file is available yet.
  - Implemented parsing of `SpawnerType` union and `BaseSpawner` fields.
  - Handles `SingleBurst`/`Repeating` spawn modes and `FixedLocation`/`RandomBox`/`RandomSphere`.
  - Applies defaults and warnings for missing/invalid fields.
- 2026-04-03: **PARTIALLY DONE** — Added visible FlatBuffer integration path via `FlatBuffer Preview` scenario.
  - Loader output is now consumed by a runtime-rendered scenario.
  - Supports rendering parsed primitive object shapes and displays loader warnings in UI.
  - Uses fallback preview data when lecturer scene file is unavailable to keep implementation progress demonstrable.
- 2026-04-03: **PARTIALLY DONE** — Added local UI mode switch for owner/material visualization in FlatBuffer preview path.
- 2026-04-03: **PARTIALLY DONE** — Implemented animated behaviour runtime update in FlatBuffer preview scenario (waypoint interpolation and path modes), enabling visible motion testing before official scene file release.
- 2026-04-03: **PARTIALLY DONE** — Added distributed ownership scaffolding in preview path:
  - Local peer identity selector (ONE..FOUR)
  - Simulated object local-owned vs remote-owned classification and UI stats.
- 2026-04-03: **PARTIALLY DONE** — Integrated depth-buffered rendering baseline required for stable simulation visualisation across camera motion.
- 2026-04-03: Runtime validation passed for ownership gating. UI counters and behavior confirm local-vs-remote simulated object classification.
- 2026-04-03: **PARTIALLY DONE** — Implemented Winsock2 UDP P2P baseline and integrated simulated object state packet send/receive in FlatBuffer Preview (owner-authoritative local update + remote state application).
- 2026-04-03: Runtime validation passed for dual-instance loopback networking (`127.0.0.1`, ports `25000`/`25001`); owner-authoritative cuboid state replicated correctly.
- 2026-04-03: **PARTIALLY DONE** — Added replicated global command path over UDP:
  - `Play`, `Pause`, `Reset`
  - `TimeStep`, `SimulationSpeed`
  - Commands now apply cross-peer in FlatBuffer Preview during runtime.
- 2026-04-03: **PARTIALLY DONE** — Added independent network tick loop (async worker thread) with adjustable Hz and runtime measured frequency display in FlatBuffer Preview.
- 2026-04-03: **PARTIALLY DONE** — Added initial thread affinity scaffolding for network thread (Windows core mask assignment).
- 2026-04-03: Runtime validation passed for replicated `Global Reset` (cross-peer reset + no crash after threading/ordering fixes).
- 2026-04-03: **PARTIALLY DONE** — Added remote-state interpolation and drift correction in FlatBuffer Preview with configurable interpolation rate and snap distance.
- 2026-04-03: **PARTIALLY DONE** — Added configurable latency/jitter/loss emulation in FlatBuffer Preview networking path for robustness testing under degraded link conditions.
- 2026-04-09: **DONE** — Completed Advanced Flocking and Steering feature:
  - Owner-authoritative networked flock simulation in `FlockingScenario`.
  - Remote interpolation/snap correction path.
  - Runtime controls for cohesion/alignment/separation/avoidance, speed/force, ownership and networking.
  - Debug counters (`Tx`/`Rx`) and reset stability fix.
- 2026-04-09: **DONE** — Scene schema/runtime integration for flocking:
  - `FlockingSettings` added to `Scene.fbs`.
  - Generated FlatBuffers code updated.
  - Loader/runtime parse path integrated.
- 2026-04-09: **PARTIALLY DONE** — Implemented flocking spatial segmentation techniques:
  - Baseline (no spatial segmentation / brute force),
  - Uniform Grid,
  - Octree.
- 2026-04-09: Added in-scenario comparison capture for:
  - physics update CPU time,
  - segmentation build/query overhead,
  - memory estimate per mode.
- 2026-04-09: Initial captured run indicates `BruteForce` currently fastest at present boid count; `UniformGrid` and `Octree` show added overhead from structure build, with `Octree` highest in this sample.


### Extended Flocking Comparison (No Segmentation vs Uniform Grid vs Octree)

#### Test Setup
- Scenario: `Flocking (Networked)` (single-owner simulation focus for timing)
- Boid count: `80`
- Integrator: `Semi-Implicit Euler`
- Time Step: `0.0167 s`
- Speed: `1.0x`
- Measurement source: in-scenario captured metrics (`Capture Sample (Current Mode)`)

#### Captured Results
| Mode         | Update CPU (ms) | Build CPU (ms) | Estimated Memory (bytes) |
|--------------|------------------|----------------|---------------------------|
| BruteForce   | 0.067            | 0.000          | 0                         |
| UniformGrid  | 0.176            | 0.103          | 1080                      |
| Octree       | 0.287            | 0.199          | 2800                      |

#### Interpretation
- At the current boid count (`80`), `BruteForce` is fastest overall.
- `UniformGrid` reduces neighbor candidate scope but introduces structure-build overhead each update.
- `Octree` has the highest overhead in this sample due to tree construction/traversal and higher memory footprint.
- For this workload, segmentation structures are not yet amortized by problem size; they are likely to become more beneficial as boid count increases.

#### Collision/Physics Update Processing Note
- Collision/neighbor interaction work is integrated into the flock update loop and captured in `Update CPU`.
- Segmentation structure overhead is captured separately in `Build CPU`.
- This allows direct comparison of total cost (`Update + Build`) across techniques.

#### Conclusion
- Requirement met: implemented and compared baseline + two segmentation techniques (`UniformGrid`, `Octree`) with timing and memory evidence.
- Current best mode at this scale: `BruteForce`.
- 2026-04-09: **PARTIALLY DONE** — Applied explicit Windows affinity masks aligned with required mapping:
  - Visualisation thread -> core 1
  - Networking worker threads -> cores 2–3
  - Simulation worker thread -> core 4+ (fallback on low-core machines)
- 2026-04-09: **PARTIALLY DONE** — Implemented asynchronous render frequency control (`Render Tick Hz`) with runtime measurement, enabling independent render/sim/network frequency demonstration.
- 2026-04-09: **PARTIALLY DONE** — Implemented runtime spawners in `FlatBufferPreviewScenario` with owner assignment (`ONE..FOUR` and `SEQUENTIAL`) and local/remote ownership stats refresh.
- 2026-04-09: **PARTIALLY DONE** — Extended distributed ownership for spawners: runtime spawned objects are now authoritatively created by spawner owner and distributed over UDP using explicit spawn packets, including `SEQUENTIAL` owner assignment rotation.
- 2026-04-11: Network receive path consolidated to network worker thread; `m_NetworkingActive` converted to atomic for thread-safe start/stop state.
- 2026-04-11: Implemented owner-driven object-vs-object collision response for simulated objects in FlatBuffer Preview runtime.
- 2026-04-11: Added network configuration quick presets (`127.0.0.1`, `25000↔25001`) and port swap UI action.
- 2026-04-11: Kept startup `RequestResync` trigger disabled to preserve stable live replication baseline.
