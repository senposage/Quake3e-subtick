# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Windows (MSVC)
Open `code/win32/msvc2017/quake3e.sln` (or msvc2019) in Visual Studio, compile the `quake3e` project.
Output goes to `code/win32/msvc2017/output/`.
For Vulkan: clean solution, select `renderervk` dependency instead of `renderer`.

### Linux / MinGW
```bash
make                                # default build (client + server)
make ARCH=x86_64                    # explicit 64-bit
make BUILD_SERVER=1 BUILD_CLIENT=0  # dedicated server only
make install DESTDIR=<game_dir>     # install to game directory
```

### CMake
```bash
cmake -B build
cmake --build build
```

Key Makefile toggles: `USE_VULKAN=1`, `USE_SDL=0`, `USE_CURL=1`, `BUILD_CLIENT=1`, `BUILD_SERVER=1`.

The engine binary is `quake3e` (client) or `quake3e.ded` (dedicated server).

---

## Project Summary

Urban Terror 4.3 (UT4.3) competitive server engine enhancement built on **Quake3e**.

Current runtime model:
- `sv_fps 60` for high-rate input sampling
- `sv_gameHz 20` for QVM game logic compatibility
- `sv_snapshotFps -1` (= sv_fps) for downstream snapshot cadence

UT4.3 QVM binaries (`qagame`, `cgame`, `ui`) are closed-source.
UT4.2 source and Ghidra notes are used as reference material only (see docs below).

---

## Repository Layout

```text
code/server/
  sv_main.c               frame loop, sv_fps/sv_gameHz decoupling, snapshot dispatch
  sv_client.c             multi-step Pmove, sv_pmoveMsec clamping, bot exclusion
  sv_init.c               cvar registration
  sv_snapshot.c           engine-side player position extrapolation/smoothing path
  sv_ccmds.c              SV_MapRestart_f game clock sync fix
  sv_antilag.c            engine-side antilag
  sv_antilag.h

code/client/
  cl_cgame.c              client time sync and serverTime control
  cl_parse.c              snapshot interval measurement and bootstrap
  cl_input.c              input/download pacing tweaks
  client.h                clientActive_t additions

code/qcommon/
  net_ip.c                net_dropsim flags update
```

---

## Engine Work — COMPLETED

### Custom CVars

| Cvar | Default | Description |
|------|---------|-------------|
| `sv_fps` | `60` | Engine tick / input sampling rate |
| `sv_gameHz` | `20` | QVM GAME_RUN_FRAME rate (keep at 20 for UT4.3 constraints) |
| `sv_snapshotFps` | `-1` | Snapshot send rate to clients (-1 = match sv_fps) |
| `sv_pmoveMsec` | `8` | Max Pmove physics step (ms) |
| `sv_busyWait` | `0` | Spin last N ms before frame |
| `sv_extrapolate` | `1` | Engine-side position correction between game frames |
| `sv_smoothClients` | `0` | Use TR_LINEAR trajectory for smoother client rendering (experimental) |
| `sv_bufferMs` | `0` | Position ring-buffer delay in ms (-1=auto, 0=off) |
| `sv_velSmooth` | `32` | Velocity smoothing window in ms; requires `sv_smoothClients 1` |
| `sv_antilagEnable` | `1` | Engine antilag on/off |
| `sv_physicsScale` | `3` | Antilag sub-ticks per game frame |
| `sv_antilagMaxMs` | `200` | Max rewind window (ms) |

### Frame Loop (sv_main.c)

```text
Com_Frame → SV_Frame(msec)
  sv.timeResidual += msec
  clamp timeResidual to frameMsec*2-1

  while timeResidual >= frameMsec (1000/sv_fps):
      timeResidual -= frameMsec
      svs.time += frameMsec
      sv.time  += frameMsec

      [antilag: record sv_physicsScale position snapshots]

      emptyFrame = qtrue
      gameTimeResidual += frameMsec
      while gameTimeResidual >= gameMsec (1000/sv_gameHz):
          gameTimeResidual -= gameMsec
          sv.gameTime += gameMsec
          SV_BotFrame(sv.gameTime)
          GAME_RUN_FRAME(sv.gameTime)
          emptyFrame = qfalse

      SV_IssueNewSnapshot()
      SV_SendClientMessages()

  SV_CheckTimeouts()
```

### Multi-Step Pmove (sv_client.c)

- Runs multiple QVM `GAME_CLIENT_THINK` calls per usercmd, clamped by `sv_pmoveMsec`.
- Consumes full real delta across steps to preserve movement and timers.
- Safety guard: if `commandTime` does not advance on first step, fallback to `single_call`.
- Bots are excluded from clamping path (`NA_BOT` check).

### Engine-Side Position Extrapolation (sv_snapshot.c)

- Player entity state is authored at game-frame cadence; at high snapshot rate this would otherwise duplicate positions.
- `SV_BuildCommonSnapshot` fixes up player positions between game frames: real players use actual `ps->origin` (updated by Pmove every usercmd); bots use velocity extrapolation (`trBase += trDelta * dt`) since their `ps->origin` only updates at game-frame boundaries.
- Guarded by player index and trajectory type checks. Velocity dead-zone check prevents idle-player vibration.
- **`sv_extrapolate` path** only runs between game frames (`sv.time > sv.gameTime`); skipped at game-frame ticks where entity state is already correct.
- **`sv_smoothClients` path** runs on **every** tick — including game-frame ticks — to ensure TR_LINEAR is never interrupted by a stray TR_INTERPOLATE snapshot (which would cause 50ms-period stutter at sv_gameHz 20).

### Antilag (sv_antilag.c)

- Engine-side replaces QVM antilag (QVM path records at 20Hz = 50ms granularity, which is too coarse for `sv_fps 60`).
- Records `sv_physicsScale` snapshots per engine tick (~5.5ms granularity at defaults: `sv_fps 60`, `sv_physicsScale 3`).
- Rewinds entity positions to `attackTime` for hit detection, then restores state after traces complete.

### sv_ccmds.c Fix

- `SV_MapRestart_f` aligns `sv.gameTime` with `sv.time` and resets residual state before restart progression.
- Warmup/restart frame progression uses the synchronized game clock.

### Why `sv_gameHz` Must Stay at 20

UT antiwarp uses hardcoded 50ms behavior in QVM logic; increasing game frame rate above 20Hz creates timing mismatch and can produce invalid movement correction. Keep `sv_gameHz` at 20 for compatibility.

### Snapshot Dispatch

`SV_IssueNewSnapshot` + `SV_SendClientMessages` are inside the `sv_fps` loop (not once per `Com_Frame`), preventing missed snapshot opportunities during scheduler overshoot.

### Client `snaps` Cvar

Engine treats `sv_snapshotFps` as authoritative in `SV_UserinfoChanged`; client `snaps` userinfo does not raise send rate beyond server policy.

---

## cgame/QVM Work — IN PROGRESS (Binary Patching Track)

Goal: improve cgame tolerance for jitter/high-latency conditions at elevated snapshot rates.

Method: Ghidra analysis of UT4.3 cgame QVM + custom pk3 replacement patches.

### Current patch priorities
1. Clamp `frameInterpolation` to avoid overshoot pops.
2. Add extrapolation fallback for `nextSnap == NULL` using `trDelta * dt`.

Reference: `docs/ghidra-cgame-patches.md`.

---

## Known Issues Status

| Issue | Status |
|-------|--------|
| End of round freeze | **FIXED** (confirmed live) |
| rcon map load broken | Likely fixed (same root cause) — needs confirmation |
| Bleed/bandage timing | **FIXED** (confirmed live) |
| Fast strafe visual stutter | **FIXED** (confirmed live) |
| Bot movement | **FIXED** (resolved) |
| cgame jitter tolerance | **IN PROGRESS** |

---

## Testing Priorities

1. rcon `map <mapname>` during live game (confirmation pass)
2. cgame jitter tests under packet delay/loss

---

## Files Modified (engine)

| File | Summary |
|------|---------|
| `code/server/sv_main.c` | Dual-rate loop, antilag recording placement, snapshot dispatch in sv_fps loop, per-tick emptyFrame reset |
| `code/server/sv_init.c` | Register sv_gameHz, sv_snapshotFps, sv_busyWait, sv_pmoveMsec, antilag cvars |
| `code/server/sv_client.c` | Multi-step Pmove with stall detection, bot exclusion, snaps policy |
| `code/server/sv_snapshot.c` | Engine-side player position extrapolation using `sv.time - sv.gameTime`, smoothing helpers |
| `code/server/sv_ccmds.c` | `SV_MapRestart_f` clock synchronization and residual reset |
| `code/server/sv_antilag.c` | Engine-side antilag implementation |
| `code/server/sv_antilag.h` | Antilag interface |

## Files Modified (client/network — jitter tolerance)

| File | Summary |
|------|---------|
| `code/client/client.h` | Added `snapshotMsec` to `clientActive_t` |
| `code/client/cl_parse.c` | Snapshot interval measurement (EMA), bootstrap from `sv_snapshotFps` in server info |
| `code/client/cl_cgame.c` | Scaled reset/fast-adjust thresholds, extrapolation detection scaling, drift correction, `serverTime` clamp, timedemo handling |
| `code/client/cl_input.c` | Download throttle scaled to snapshot interval |
| `code/qcommon/net_ip.c` | `net_dropsim` flagged as `CVAR_CHEAT` |

### Jitter Tolerance System (cl_cgame.c)

**Snapshot interval tracking:** `cl.snapshotMsec` uses snapshot-arrival EMA (`75% old / 25% new`), initialized from `sv_snapshotFps`, clamped to `[8, 100]`.

**Scaled time sync thresholds:**
- RESET_TIME: `snapshotMsec * 10` (min 200ms) — hard reset on large desync.
- Fast adjust: `snapshotMsec * 2` (min 50ms) — halve the difference instead of hard snapping.
- Extrapolation detection: `snapshotMsec / 3` (clamped `[3, 16]`) — flag drift correction when client time is running ahead.
- Drift pullback: stronger at high snapshot rates (`-4ms/frame` high-rate vs `-2ms/frame` vanilla-rate behavior).

**serverTime clamp:** `cl.serverTime` capped to `cl.snap.serverTime + cl.snapshotMsec` to prevent interpolation overshoot/snap-back behavior.

### Antiwarp Analysis

g_antiwarp remains safe with `sv_fps` elevated **when** `sv_gameHz` remains 20:
- game-side checks operate at game-frame cadence
- 50ms antiwarp assumptions remain aligned
- triggers occur on real command gaps rather than normal high-rate server ticks

---

## Reference Docs

- `CLAUDE_CONTEXT.md` — full architecture/rationale.
- `docs/CODEMAP.md` — code navigation map.
- `CVARS.md` — cvar catalog and interaction notes.
- `docs/ghidra-cgame-patches.md` — QVM patch handoff.
- `docs/debug-session-2026-02-26-cl_snapScaling-stutter.md` — cl_snapScaling investigation.
- `docs/debug-session-2026-02-27-smoothing-jitter.md` — smoothing jitter investigation.
