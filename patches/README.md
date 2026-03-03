# Patches — Quake3e-subtick + FTWGL Fork

These patches port all ftwgl/urt and subtick features from this fork to upstream
[ec-/Quake3e](https://github.com/ec-/Quake3e). They are generated against
upstream `main` and must be applied **in order**.

## Applying

```bash
# Fork and clone upstream quake3e
git clone https://github.com/<you>/Quake3e.git
cd Quake3e

# Apply all patches in order
git am patches/0001-ftwgl-base-server-infrastructure.patch
git am patches/0002-subtick-engine-side-antilag.patch
git am patches/0003-ftwgl-client-changes.patch
git am patches/0004-ftwgl-network-and-common.patch
git am patches/0005-ftwgl-build-system.patch
```

Or apply all at once:

```bash
git am patches/*.patch
```

If a patch has conflicts due to upstream changes:

```bash
git am --abort                    # cancel
git am --3way patches/XXXX.patch  # retry with 3-way merge
```

## Patch Contents

### 0001 — Server Infrastructure

All server-side changes: ftwgl fork infrastructure (USE_MV multiview, USE_AUTH,
USE_SERVER_DEMO) **plus** subtick engine features.

| File | Changes |
|------|---------|
| `server.h` | gameTime/gameTimeResidual fields, awLastThinkTime, subtick cvar externs, USE_MV/USE_AUTH/USE_SERVER_DEMO fields, API changes |
| `sv_main.c` | Dual-rate frame loop (sv_fps/sv_gameHz), engine-side antiwarp, snapshot dispatch, ftwgl cvar defs, USE_MV hooks |
| `sv_init.c` | Cvar registration (sv_gameHz, sv_snapshotFps, sv_pmoveMsec, sv_busyWait, sv_extrapolate, sv_smoothClients, sv_bufferMs, sv_velSmooth, sv_antiwarp*, antilag), USE_MV/USE_AUTH init |
| `sv_client.c` | Multi-step Pmove (sv_pmoveMsec), snaps policy (sv_snapshotFps), awLastThinkTime tracking, USE_MV/USE_AUTH client handling |
| `sv_snapshot.c` | Engine-side position extrapolation, TR_LINEAR smoothing, velocity smoothing, per-client ring buffer, USE_MV snapshot changes |
| `sv_ccmds.c` | MapRestart clock sync (sv.gameTime = sv.time), USE_MV/USE_SERVER_DEMO commands |
| `sv_bot.c` | Bot snapshot rate fix |

### 0002 — Engine-Side Antilag (NEW files)

| File | Changes |
|------|---------|
| `sv_antilag.h` | Antilag interface — SV_AntilagInit, SV_AntilagRecord, SV_AntilagRewind, SV_AntilagRestore |
| `sv_antilag.c` | Full antilag implementation — position recording at sv_physicsScale sub-ticks, rewind/restore for hit detection |

**Cvars:** `sv_antilagEnable`, `sv_physicsScale`, `sv_antilagMaxMs`

### 0003 — Client Changes

| File | Changes |
|------|---------|
| `client.h` | snapshotMsec field in clientActive_t, USE_MV client fields |
| `cl_cgame.c` | Scaled jitter tolerance (RESET_TIME, fast-adjust, extrapolation detection, drift correction), serverTime clamp |
| `cl_parse.c` | Snapshot interval measurement (EMA), bootstrap from sv_snapshotFps, USE_MV parsing |
| `cl_input.c` | Download throttle scaled to snapshot interval, USE_MV input |

### 0004 — Network and Common

| File | Changes |
|------|---------|
| `net_ip.c` | net_dropsim CVAR_CHEAT → CVAR_TEMP, structural changes |
| `net_chan.c` | NET_FlushPacketQueue force-flush on delay=0, USE_MV netchan |
| `common.c` | cl_packetdelay CVAR_CHEAT → CVAR_TEMP, ftwgl fork common changes |

### 0005 — Build System

| File | Changes |
|------|---------|
| `Makefile` | sv_antilag.c/h added to server build, USE_MV/USE_AUTH/USE_SERVER_DEMO build flags |
| `CMakeLists.txt` | Same additions for CMake |

## Feature Summary

### Subtick Features
- **sv_gameHz** — Decouple QVM game frame rate from engine tick rate
- **sv_snapshotFps** — Independent snapshot send rate
- **sv_pmoveMsec** — Multi-step Pmove for consistent physics
- **sv_extrapolate** — Position correction between game frames
- **sv_smoothClients** — TR_LINEAR trajectory for smooth rendering
- **sv_bufferMs** — Per-client position ring buffer
- **sv_velSmooth** — Velocity smoothing for TR_LINEAR
- **sv_antiwarp** — Engine-side antiwarp (replaces QVM g_antiwarp)
- **sv_antilag** — Engine-side antilag with sub-tick recording
- **sv_busyWait** — Spin-wait for precise frame timing
- **Client jitter tolerance** — Scaled time sync thresholds for high snapshot rates
- **MapRestart clock sync** — Prevents sv.gameTime / sv.time desync on restart

### FTWGL Fork Features
- **USE_MV** — Multiview spectator support
- **USE_AUTH** — Server authentication system
- **USE_SERVER_DEMO** — Server-side demo recording
- Various structural improvements and API changes

## Recommended Setup (UT 4.3.4)

```
sv_fps 60
sv_gameHz 0
sv_snapshotFps -1
sv_pmoveMsec 8
sv_antiwarp 2
sv_antiwarpDecay 150
sv_smoothClients 1
sv_velSmooth 32
sv_antilagEnable 1
```
