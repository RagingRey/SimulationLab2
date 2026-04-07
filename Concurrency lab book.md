## Final Lab (700120) - Concurrency + Integration Tracking

### Current Feature Status
- FlatBuffers scene loading with defaults: `PARTIALLY DONE`
- Peer-to-peer Winsock2 networking: `PARTIALLY DONE`
- Distributed ownership + replication: `PARTIALLY DONE`
- Async loops (render/network/simulation at independent Hz): `NOT STARTED`
- Thread affinity mapping: `NOT STARTED`
- Local/global UI split for networked simulator: `PARTIALLY DONE`

### Implemented So Far (Codebase Snapshot)
- Existing Vulkan render loop and frame sync in `SandboxApplication`: `DONE`
- Existing scenario switching UI in `ImGuiLayer`: `DONE`
- Existing local camera control + camera presets: `DONE`
- Existing fixed-step simulation controls (pause/step/timestep/speed): `DONE`
- Network/distributed systems: `PARTIALLY DONE`

### Evidence / Notes
- Current architecture is single-process, single-node simulation with scenario abstraction.
- UI now supports local ownership selection plus replicated global commands in the FlatBuffer Preview networking path.
- Next implementation milestone is async thread orchestration (render/network/simulation split + affinity mapping).

### Progress Log
- 2026-03-31: Added Final Lab tracking section and baseline status
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