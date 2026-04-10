# Final Lab Progress Tracker

## Status Legend
- `NOT STARTED`
- `PARTIALLY DONE`
- `DONE`
- `FULLY DONE`

## Core Simulation Features
- Scenes loaded from FlatBuffers with defaults: `PARTIALLY DONE`
- Named scene switching at runtime (global): `PARTIALLY DONE`
- Camera switching + local camera controls: `PARTIALLY DONE`
- Physics objects with shape/material/behaviour/collision type: `PARTIALLY DONE`
- Material interactions table (restitution/static/dynamic friction): `NOT STARTED`
- Simulated rigid body dynamics (linear/angular): `PARTIALLY DONE`
- Animated objects (waypoints/easing/path mode): `PARTIALLY DONE`
- Spawners (single burst/repeating, random ranges): `PARTIALLY DONE`

## Core Concurrency Features
- Peer-to-peer networking (Winsock2): `PARTIALLY DONE`
- Distributed ownership (ONE/TWO/THREE/FOUR): `PARTIALLY DONE`
- Owner-driven simulation + per-frame state replication: `PARTIALLY DONE`
- Async component frequencies (render/network/simulation): `DONE`
- Thread affinity mapping (core 1 / 2-3 / 4+): `DONE`

## UI Requirements
- Local UI controls: `PARTIALLY DONE`
- Global UI controls replicated across peers: `PARTIALLY DONE`

## Robustness
- Drift correction / interpolation: `PARTIALLY DONE`
- Latency/loss resilience (100ms ± 50ms, 20% loss): `PARTIALLY DONE`

## Advanced Feature
- Feature selected: `DONE`
- Schema extension added: `DONE`
- Runtime controls/debug visualization: `DONE`

## Extended Advanced Feature
- Flocking spatial segmentation (No segmentation vs Uniform Grid vs Octree): `DONE`
- Performance comparison (memory/collision/physics update processing): `DONE`

## Implementation Log
- 2026-03-31: Initialized final-lab progress tracker.
- 2026-03-31: Confirmed single lab book workflow (`../Concurrency lab book.md`).
- 2026-04-01: Added `SceneRuntime` and `SceneLoaderFlatBuffer` (Scene/Cameras/Materials/Interactions/Objects parsing with default fallback + warnings). Spawner parsing still pending.
- 2026-04-03: Added fallback scene bootstrap. App now attempts FlatBuffer scene load from candidate paths and falls back to built-in scenarios when unavailable, so development is not blocked.
- 2026-04-03: Extended `SceneLoaderFlatBuffer` to parse spawner unions (`Sphere/Cylinder/Capsule/Cuboid`) including base settings, spawn mode, location mode, velocity ranges, material and owner defaults.
- 2026-04-03: Added visible `FlatBuffer Preview` scenario. Parsed FlatBuffer object data now drives rendered geometry (sphere/cylinder/capsule/plane/cuboid) with fallback preview when scene file is absent.
- 2026-04-03: Added local display toggle in `FlatBuffer Preview` to switch between owner-based colouring and material-based colouring.
- 2026-04-03: Added animated waypoint path update (STOP/LOOP/REVERSE + LINEAR/SMOOTHSTEP) in FlatBuffer preview runtime path.
- 2026-04-03: Added ownership groundwork in FlatBuffer preview (local peer selector, local/remote simulated ownership flags and counts).
- 2026-04-03: Added depth buffering to Vulkan dynamic rendering path (depth image/view creation, pipeline depth test/write, depth attachment in rendering info, swapchain recreate/cleanup integration).
- 2026-04-03: Runtime validation passed for ownership gating. UI counters and behavior confirm local-vs-remote simulated object classification.
- 2026-04-03: Added UDP Winsock2 baseline (`NetworkPeer`) and integrated state send/receive in FlatBuffer Preview.
- 2026-04-03: Validated dual-instance loopback replication (`127.0.0.1`, 25000↔25001) for owner-driven simulated cuboid state transfer.
- 2026-04-03: Added global command replication over UDP for `Play`, `Pause`, `Reset`, `TimeStep`, and `Speed`; verified sync behavior across peers.
- 2026-04-03: Added asynchronous network worker loop in `FlatBufferPreviewScenario` with independent target frequency and measured Hz reporting. Added initial thread affinity scaffolding for network worker.
- 2026-04-03: Fixed replicated global reset path ordering and thread-lock interaction; `Global Reset` now safely applies across peers without runtime abort.
- 2026-04-03: Added remote-state drift correction/interpolation in `FlatBufferPreviewScenario` (target-state smoothing + snap threshold controls in UI).
- 2026-04-03: **PARTIALLY DONE** — Added configurable latency/jitter/loss emulation in FlatBuffer Preview networking path for robustness testing under degraded link conditions.
- 2026-04-03: Validated robustness under emulated degraded link; interpolation reduced visible jitter while maintaining convergence.
- 2026-04-09: **DONE** — Extended `Scene.fbs`/generated schema with `FlockingSettings` and integrated scene-runtime loader parsing.
- 2026-04-09: **PARTIALLY DONE** — Added `Flocking (Networked)` scenario with owner-authoritative boid simulation, remote interpolation, live runtime tuning, and packet counters (`Tx/Rx`).
- 2026-04-09: Fixed flock reset re-entrant lock crash in `FlockingScenario` UI path.
- 2026-04-09: **PARTIALLY DONE** — Added `UniformGrid` and `Octree` neighbor-search modes in `FlockingScenario` with runtime CPU/memory capture UI.
- 2026-04-09: Captured baseline comparison samples (`BruteForce` vs `UniformGrid` vs `Octree`) showing update/build/memory metrics in-app.
- 2026-04-09: **PARTIALLY DONE** — Updated thread affinity mapping toward Final Lab core allocation: visualisation pinned to core 1, networking threads to cores 2–3, simulation thread to core 4+ (with low-core fallback masks).
- 2026-04-09: **PARTIALLY DONE** — Added independent graphics/render frequency control (`Render Tick Hz`) and measured render rate reporting via ImGui, decoupled from simulation/network loop rates.
- 2026-04-09: **PARTIALLY DONE** — Added runtime spawner execution in `FlatBufferPreviewScenario` (SingleBurst/Repeating + Fixed/RandomBox/RandomSphere) with ownership assignment including `SEQUENTIAL` round-robin.
- 2026-04-09: **PARTIALLY DONE** — Added owner-authoritative spawner distribution path in `FlatBufferPreviewScenario`: only spawner authority creates runtime spawned objects and distributes spawn events across peers before state replication.
- 2026-04-11: Stabilized network threading path by moving command receive to network worker and making `m_NetworkingActive` atomic.
- 2026-04-11: Added owner-side simulated object-vs-object collision response in `FlatBufferPreviewScenario::OnUpdate`.
- 2026-04-11: Added networking ease-of-configuration presets (`Preset A`, `Preset B`, `Swap Ports`) in FlatBuffer Preview UI.
- 2026-04-11: Startup auto-`RequestResync` left disabled for stability in mark-focused baseline path.