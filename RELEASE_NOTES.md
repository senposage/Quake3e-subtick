# Release Notes — v1.0

## Feature Highlights

### 🔊 Audio

- **OpenAL Soft 1.25.1** bundled on Windows — no separate install required.
- **Per-source suppression** — suppressed weapons apply an individual HF lowpass filter per source instead of a global volume duck.
- **Combat audio cues** — tinnitus on nearby explosions, distinct head-hit sound, health-fade audio degradation.
- **Per-category volume mixer** — independent `weapon`, `suppressed`, `world`, `ambient`, `UI`, and `music` channels (0–10 scale).
- **EFX effects chain** — reverb, echo, and explosion bloom via `alEFX`.
- **WASAPI backend (Windows)** — dmaHD ported from DirectSound to WASAPI for bit-perfect, low-latency output.
- **Spatial occlusion** — multi-hop BSP corridor probing with IIR-smoothed HRTF redirection and a per-frame trace budget to prevent audio stalls on busy servers.
- New cvars: `s_alEFX`, `s_alLocalSelf`, `s_alDirectChannels`, `s_alReset`, `s_alTrace`, `s_devices`, `s_alExtraVolEntry1`–`8`.

### ⏱️ Client: Adaptive Timing

- `cl_adaptiveTiming 2` — proportional drift-correction mode for faster equilibrium convergence.
- Automatically disabled on vanilla Q3 servers; re-activates on FTWGL/URT servers via `sv_allowClientAdaptiveTiming`.

### 🎯 Server: Antilag & Smoothing

- **Engine-side shadow antilag** (`sv_antilag`) — fire-time rewind uses `lastUsercmd.serverTime` for correct per-client compensation at any `sv_fps`.
- **`snapshotMsec` compensation** — rewind accounts for client interpolation offset so the rewound position matches exactly what the shooter saw.
- **`sv_smoothClients`** — server stamps `TR_LINEAR` snapshots with velocity, enabling smooth entity interpolation at 60/125 Hz without QVM changes.
- **`sv_antiwarp 2`** — frame-rate-aware engine antiwarp (decay mode) replaces the QVM's hardcoded 50 ms injection, safe at any `sv_gameHz`.
- **`sv_gameHz 0`** — recommended default; decouples snapshot rate from game-frame rate for consistent subtick accuracy.
- **QVM runtime patches** (`VM_URT43_CgamePatches`) — three in-memory binary patches fix the UrT 4.3 cgame jitter, first-frame crash, and broken `TR_INTERPOLATE` extrapolation without modifying the `.qvm` file.

### 🚀 CI / Release Pipeline

- Automated builds for **Windows x64/x86** and **Linux x86/x64** on every push to `master`.
- `release.yml` workflow tags a release and attaches platform zips directly to the GitHub Releases page.
