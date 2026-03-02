# Custom Cvar Reference — Quake3e-subtick

All custom cvars added to this engine fork.

---

## Recommended Setup — UT 4.3.4

```
sv_fps 60
sv_gameHz 0              // default — game frames fire at sv_fps rate
sv_snapshotFps -1        // match sv_fps
sv_pmoveMsec 8           // 125fps-equivalent physics steps
sv_antiwarp 2            // engine-side antiwarp with decay
sv_antiwarpDecay 150     // tune higher (e.g. 225) for lossy networks
sv_extrapolate 1         // default — harmless no-op at sv_gameHz 0
sv_smoothClients 1       // TR_LINEAR trajectory for cgame
sv_bufferMs 0            // no position delay (not needed at sv_gameHz 0)
sv_velSmooth 32          // velocity smoothing for TR_LINEAR
sv_antilagEnable 1       // engine-side antilag
```

**Why sv_gameHz 0:** At sv_gameHz 0, `GAME_RUN_FRAME` fires every engine tick. Entity state is always fresh — `sv_extrapolate` and `sv_bufferMs` become no-ops. `sv_smoothClients` still adds value via the TR_LINEAR trajectory type. Engine `sv_antiwarp` replaces QVM `g_antiwarp` and removes the sv_gameHz 20 constraint.

For legacy setups (UT 4.0-4.2, QVM g_antiwarp), see [SV_GAMEHZ.md](../docs/project/SV_GAMEHZ.md).

---

## Core Server Cvars

### sv_fps
**Default:** 60 | **Flags:** CVAR_TEMP, CVAR_PROTECTED | **File:** sv_init.c

Engine tick and input sampling rate (Hz). Controls how often the server processes usercmds, records antilag snapshots, and sends network snapshots.

**How:** `sv_main.c:SV_Frame()` outer `while` loop runs at sv_fps rate. `sv.time` and `svs.time` advance by `1000/sv_fps` per tick.

**Interpolation windows:** sv_fps 20=50ms, 60=16.7ms, 80=12.5ms, 100=10ms, 125=8ms. Higher rates = tighter windows = less tolerance for network jitter.

---

### sv_snapshotFps
**Default:** -1 | **Flags:** CVAR_ARCHIVE, CVAR_SERVERINFO | **File:** sv_init.c

Max snapshot send rate to clients. `-1` = match `sv_fps` (live-tracks changes). `0` = per-client `snaps` userinfo (vanilla Q3). `>0` = explicit rate capped to `sv_fps`. Fully authoritative — ignores client `snaps` cvar.

**How:** `sv_client.c:SV_UserinfoChanged()` computes `cl->snapshotMsec = 1000 / min(sv_snapshotFps, sv_fps)`.

---

### sv_pmoveMsec
**Default:** 8 | **Flags:** CVAR_ARCHIVE, CVAR_SERVERINFO | **File:** sv_init.c

Maximum physics step size (ms). 8ms = 125fps equivalent Pmove. Set to 0 to disable.

**Why:** Equalizes physics behavior regardless of client framerate (jump height, strafe speed).

**How:** `sv_client.c:SV_ClientThink()` fires multiple `GAME_CLIENT_THINK` calls per usercmd, each clamped to `sv_pmoveMsec` ms. Bots excluded. Safety: `goto single_call` if `commandTime` doesn't advance (intermission/pause).

---

### sv_busyWait
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Spin for the last N milliseconds of each frame instead of sleeping. Eliminates OS scheduler jitter at the cost of ~1 CPU core at 100%. The `timeResidual` clamping fix makes this mostly unnecessary.

---

## Antiwarp

### sv_antiwarp
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Engine-side antiwarp replacement for QVM `g_antiwarp`. Injects blank commands for lagging clients before each `GAME_RUN_FRAME`.

- **0** = disabled. Use QVM `g_antiwarp` if needed (requires `sv_gameHz 20`).
- **1** = constant. Keep last inputs indefinitely (QVM-style). Player continues running with stale inputs until real commands resume.
- **2** = decay. Three-phase approach:
  - **Extrapolate** (first `tolerance + sv_antiwarpExtra` ms): keep last inputs. Brief jitter absorbed transparently.
  - **Decay** (next `sv_antiwarpDecay` ms): linearly blend movement inputs toward zero.
  - **Friction** (after decay): inputs zeroed, Pmove friction coasts to natural stop.

When enabled (1 or 2), forces `g_antiwarp 0` to prevent double injection.

**Why engine-side:** QVM antiwarp hardcodes `serverTime += 50`, forcing `sv_gameHz 20`. Engine antiwarp uses `gameMsec` (actual frame duration) — works at any `sv_fps` / `sv_gameHz`, including `sv_gameHz 0`.

**How:** Before each `GAME_RUN_FRAME`, checks each active non-bot client. If `sv.gameTime - awLastThinkTime > tolerance`, injects blank command via `GAME_CLIENT_THINK`. 100% server-side, works with vanilla clients.

---

### sv_antiwarpTol
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Tolerance in ms before blank injection fires. `0` = auto (game frame duration: `1000/sv_gameHz` or `1000/sv_fps`). `>0` = explicit threshold. **Requires `sv_antiwarp >= 1`.**

---

### sv_antiwarpExtra
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Extrapolation window for `sv_antiwarp 2` (ms). How long full movement inputs are kept after the tolerance gap before decay starts.

- **0** = auto (uses `sv_antiwarpTol` value — decay starts at `2 × tolerance`).
- **>0** = explicit window in ms. Larger = more jitter absorbed, but player runs longer on stale inputs during real lag.

**Requires `sv_antiwarp 2`.**

---

### sv_antiwarpDecay
**Default:** 150 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Decay duration for `sv_antiwarp 2` (ms). After the extrapolation window, movement inputs are linearly decayed to zero. Then Pmove friction decelerates to a natural stop. `0` = skip decay, zero inputs immediately.

**Example at defaults** (`sv_fps 60`, `sv_antiwarpTol 0` auto=16ms, `sv_antiwarpExtra 0` auto=16ms, `sv_antiwarpDecay 150`):
- 0-16ms gap: no injection (within tolerance)
- 16-32ms: full movement extrapolation (jitter absorbed)
- 32-182ms: inputs decay to zero (smooth deceleration)
- 182ms+: friction only (natural stop)

**Tuned for lossy networks** (`sv_antiwarpExtra 50`, `sv_antiwarpDecay 225`):
- 0-16ms: no injection
- 16-66ms: full extrapolation (absorbs moderate packet loss)
- 66-291ms: decay to zero
- 291ms+: friction only

**Requires `sv_antiwarp 2`.**

---

## Antilag

### sv_antilagEnable
**Default:** 1 | **Flags:** CVAR_ARCHIVE | **File:** sv_antilag.c

Engine-side antilag hit detection. Records positions at `sv_physicsScale` sub-ticks per engine tick (~5.5ms granularity at defaults). On hit detection, rewinds entity positions to `attackTime`, traces, restores.

### sv_physicsScale
**Default:** 3 | **Flags:** CVAR_ARCHIVE | **File:** sv_antilag.c

Antilag sub-tick snapshots per engine tick. At sv_fps 60 with scale 3: records every ~5.5ms.

### sv_antilagMaxMs
**Default:** 200 | **Flags:** CVAR_ARCHIVE | **File:** sv_antilag.c

Maximum rewind window (ms). Players with ping above this get no antilag compensation.

---

## Smoothing & Position Correction

> **At sv_gameHz 0 (recommended):** `sv_extrapolate` and `sv_bufferMs` are no-ops — entity state is always fresh because `GAME_RUN_FRAME` fires every tick. `sv_smoothClients` still provides value via the TR_LINEAR trajectory type. `sv_velSmooth` smooths velocity for TR_LINEAR.

### sv_extrapolate
**Default:** 1 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Engine-side position correction for snapshots between game frames.

- **0** = disabled. At sv_gameHz > 0, all players stutter (entity state only updates at game-frame rate).
- **1** = enabled. Real players: reads `ps->origin` (exact). Bots: velocity extrapolation.

**At sv_gameHz 0:** harmless no-op — `BG_PlayerStateToEntityState` already runs every tick. Leave at 1 (default).

**How:** `sv_snapshot.c:SV_BuildCommonSnapshot()` Phase 1. Velocity dead-zone check `DotProduct > 100.0` prevents idle player vibration.

---

### sv_smoothClients
**Default:** 1 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Player entity trajectory type in snapshots.

- **0** = `TR_INTERPOLATE`. cgame lerps between snapshot positions.
- **1** = `TR_LINEAR`. cgame evaluates `trBase + trDelta * dt` continuously. Smoother for direction changes.

**At sv_gameHz 0:** the primary value is the trajectory type change, not position correction. Recommended: `1`.

**How:** `sv_snapshot.c:SV_BuildCommonSnapshot()` Phase 2. Idle players stay TR_INTERPOLATE (dead-zone check). Runs every tick to prevent stutter from trajectory type interruptions.

---

### sv_bufferMs
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Per-client position ring buffer with configurable delay. Requires `sv_extrapolate 1` or `sv_smoothClients 1`.

- **0** = disabled. No delay, lowest latency.
- **-1** = auto (`1000/sv_fps` — one snapshot interval).
- **1-100** = manual delay in ms.

**At sv_gameHz 0:** not useful. Positions are already fresh every tick — the buffer just adds latency for no benefit. Leave at 0.

**At sv_gameHz > 0:** provides smoothing between 20Hz game-frame position updates. Auto (-1) is the minimum for clean interpolation.

**How:** 32-slot ring buffer per client in `sv_snapshot.c`. Capacity: 256ms at sv_fps 125, 533ms at sv_fps 60.

---

### sv_velSmooth
**Default:** 32 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Velocity smoothing window (ms). Requires `sv_smoothClients 1`.

- **0** = disabled (raw velocity).
- **1-100** = averaging window. At sv_fps 60: ~2 ticks of velocity data.

Averages velocity over the last N ms to reduce direction-change sawtooth. No position latency added — only the velocity vector is smoothed.

---

## Legacy: sv_gameHz

**Default:** 0 | **Flags:** CVAR_ARCHIVE, CVAR_SERVERINFO | **File:** sv_init.c

Rate at which `level.time` advances and `GAME_RUN_FRAME` fires. `0` = disabled (falls back to sv_fps).

| Target | sv_gameHz | Notes |
|--------|-----------|-------|
| **UT 4.3.4 + sv_antiwarp** | **0** (recommended) | No constraints — engine antiwarp is frame-rate aware |
| UT 4.3.4 + QVM g_antiwarp | 20 | Required — QVM hardcodes 50ms injection |
| UT 4.0-4.2 | 20 | Required — game code intolerant of non-20Hz |

For the full sv_gameHz reference (how it works, position correction pipeline, combination matrix), see [SV_GAMEHZ.md](../docs/project/SV_GAMEHZ.md).

---

## Client-Side Cvars

### Removed: cl_snapScaling
Removed after testing — created serverTime oscillation and stutter at high snapshot rates. See `docs/debug-session-2026-02-26-cl_snapScaling-stutter.md`.

---

## Bug Fixes (no cvar — always active)

| Fix | File | What |
|-----|------|------|
| Snapshot dispatch in sv_fps loop | sv_main.c | `SV_IssueNewSnapshot` + `SV_SendClientMessages` inside the tick loop. Prevents 32ms gaps from OS jitter. |
| gameTimeResidual preserved | sv_main.c | NOT reset on sv_fps change — prevents bot stutter. |
| sv_snapshotFps->modified cleared | sv_main.c | Was never cleared → stale flag caused recalculation every frame. |
| Listen server SV_BotFrame | sv_main.c | Both dedicated and listen servers call `SV_BotFrame(sv.gameTime)` inside sv_gameHz loop. |
| Bot snapshot rate | sv_bot.c | Uses `min(sv_snapshotFps, sv_fps)` instead of raw `sv_fps`. |
| SV_MapRestart_f clock sync | sv_ccmds.c | Syncs `sv.gameTime = sv.time` before `SV_RestartGameProgs()`. |
| net_dropsim / cl_packetdelay CVAR_TEMP | net_ip.c, common.c | Changed from `CVAR_CHEAT` to `CVAR_TEMP` for testing. |

---

## Smoothing Configuration Guide

> This section applies primarily to **sv_gameHz > 0** setups where position correction matters. At **sv_gameHz 0** (recommended), use `sv_smoothClients 1`, `sv_bufferMs 0`, `sv_velSmooth 32`.

`sv_bufferMs`, `sv_smoothClients`, and `sv_velSmooth` compose as stages in a pipeline. Phase 1 resolves the position source (`sv_bufferMs`); Phase 2 resolves the trajectory type (`sv_smoothClients`) and velocity (`sv_velSmooth`).

### Combination Table

| sv_smoothClients | sv_bufferMs | sv_velSmooth | Position Source | Trajectory | Description |
|---|---|---|---|---|---|
| 0 | 0 | any | Current | TR_INTERPOLATE | **Default.** Lowest latency. |
| 0 | -1 | any | Delayed (auto) | TR_INTERPOLATE | One-interval position delay. Requires `sv_extrapolate 1`. |
| 1 | 0 | 0 | Current | TR_LINEAR | Continuous trajectory, raw velocity. |
| 1 | 0 | 1-100 | Current | TR_LINEAR | **Recommended at sv_gameHz 0.** Continuous trajectory + smoothed velocity. |
| 1 | -1 | 1-100 | Delayed (auto) | TR_LINEAR | Delayed base + smoothed velocity. Best for sv_gameHz 20. |
