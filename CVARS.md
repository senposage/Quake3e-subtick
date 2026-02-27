# Custom Cvar Reference — Quake3e-subtick

All custom cvars added to this engine fork, organized by what they fix and how they work.

---

## Server-Side Cvars

### sv_fps
**Default:** 60 | **Flags:** CVAR_TEMP, CVAR_PROTECTED | **File:** sv_init.c

Engine tick and input sampling rate (Hz). Controls how often the server processes usercmds, records antilag snapshots, and sends network snapshots. Higher = finer input resolution.

**Why:** Vanilla Q3 ties everything to a single tick rate. We decouple input sampling (sv_fps) from game logic (sv_gameHz) so the QVM runs at its expected 20Hz while the engine processes inputs at 60Hz+.

**How:** `sv_main.c:SV_Frame()` outer `while` loop runs at sv_fps rate. `sv.time` and `svs.time` advance by `1000/sv_fps` per tick.

**Interpolation windows:** sv_fps 20=50ms, 60=16.7ms, 90=11.1ms, 125=8ms. Higher rates = tighter windows = less tolerance for network jitter.

---

### sv_gameHz
**Default:** 20 | **Flags:** CVAR_ARCHIVE, CVAR_SERVERINFO | **File:** sv_init.c

Rate at which `level.time` advances and `GAME_RUN_FRAME` fires. **MUST stay at 20** for UT4.3 — the QVM has a hardcoded `serverTime += 50` antiwarp injection in `g_active.c` that assumes 50ms game frames.

**Why:** Decouples game logic rate from engine tick rate so sv_fps can be raised without breaking QVM timers (bleed, bandage, antiwarp, inactivity).

**How:** Inner `while` loop in `SV_Frame()` accumulates `sv.gameTimeResidual` and fires `GAME_RUN_FRAME(sv.gameTime)` when it reaches `1000/sv_gameHz`.

---

### sv_snapshotFps
**Default:** 60 | **Flags:** CVAR_ARCHIVE, CVAR_SERVERINFO | **File:** sv_init.c

Max snapshot send rate to clients. Server ignores client `snaps` userinfo — this is fully authoritative.

**Why:** Ensures all clients receive snapshots at a consistent rate controlled by the server, not by individual client settings.

**How:** `sv_client.c:SV_UserinfoChanged()` computes `cl->snapshotMsec = 1000 / min(sv_snapshotFps, sv_fps)`. Client `snaps` cvar is bypassed entirely.

---

### sv_pmoveMsec
**Default:** 8 | **Flags:** CVAR_ARCHIVE, CVAR_SERVERINFO | **File:** sv_init.c

Maximum physics step size (ms). 8ms = 125fps equivalent Pmove. Set to 0 to disable.

**Why:** Without this, a player at 333fps gets 3ms Pmove steps while a player at 60fps gets 16ms steps — different physics behavior (jump height, strafe speed). Clamping to 8ms equalizes everyone.

**How:** `sv_client.c:SV_ClientThink()` fires multiple `GAME_CLIENT_THINK` calls per usercmd, each clamped to `sv_pmoveMsec` ms. Full real delta consumed across all steps so timers (bleed, bandage) stay correct. Bots excluded (they use 50ms steps from `level.time`). Safety: if `commandTime` doesn't advance on first call (intermission/pause), falls through to single call via `goto single_call`.

---

### sv_extrapolate
**Default:** 1 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Engine-side position correction for high sv_fps snapshots.

- **0** = disabled. Vanilla behavior: `BG_PlayerStateToEntityState` only runs at sv_gameHz (20Hz), so 3 consecutive 60Hz snapshots have identical player positions → visible stutter.
- **1** = enabled. Real players: reads actual `ps->origin` from the game module (updated every usercmd by Pmove). Bots: velocity-based extrapolation (`trBase += trDelta * dt`), which is accurate because bot AI only changes direction at game frame boundaries.

**Why:** At sv_fps 60 with sv_gameHz 20, the entity state (`ent->s`) only updates every 3rd engine tick. Without correction, clients see players teleporting every 50ms instead of moving smoothly every 16ms.

**How:** `sv_snapshot.c:SV_BuildCommonSnapshot()` checks `sv.time - sv.gameTime > 0` (between game frames). For real players, copies `ps->origin` → `es->pos.trBase` and `ps->velocity` → `es->pos.trDelta`. Velocity dead-zone check `DotProduct(velocity, velocity) > 100.0` prevents idle player vibration from Pmove ground snapping micro-oscillations.

---

### sv_smoothClients
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

**EXPERIMENTAL.** Changes player entity trajectory type from `TR_INTERPOLATE` to `TR_LINEAR` in snapshots.

- **0** = `TR_INTERPOLATE` (default). cgame lerps between two snapshot positions. Simple, reliable, but can show sawtooth artifacts on rapid direction changes because the lerp target is always one snapshot behind.
- **1** = `TR_LINEAR`. cgame calls `BG_EvaluateTrajectory()` which computes `position = trBase + trDelta * (cg.time - trTime) * 0.001` continuously. No snap-to-snap lerping — the trajectory evaluates smoothly at any render timestamp.

**Why:** With `TR_INTERPOLATE`, cgame linearly interpolates between the previous and current snapshot positions. When a player reverses direction, the interpolation target is wrong until the next snapshot arrives — visible as a brief drift in the wrong direction. `TR_LINEAR` lets cgame compute position from velocity at any time, potentially smoother for direction changes.

**How:** `sv_snapshot.c:SV_BuildCommonSnapshot()` sets `es->pos.trType = TR_LINEAR` and `es->pos.trTime = sv.time`. Requires `sv_extrapolate 1`. When `sv_smoothClients 1` is enabled, it uses the position resolved by `sv_bufferMs` (Phase 1) as the base, then applies velocity smoothing from `sv_velSmooth` (if enabled) for `trDelta`. The two settings now compose: `sv_bufferMs` controls the position source, `sv_smoothClients` controls the trajectory type.

**Safety:** Idle players (velocity near zero) are NOT switched to TR_LINEAR — they stay TR_INTERPOLATE to prevent extrapolation drift/vibration. The DotProduct > 100.0 dead-zone check applies to both smoothed velocity (ring buffer path) and raw velocity (fallback path). Pmove operates on playerState only and does NOT interact with entityState trajectory type changes.

---

### sv_busyWait
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Spin for the last N milliseconds of each frame instead of sleeping. Eliminates OS scheduler jitter at the cost of ~1 CPU core at 100%.

**Why:** OS sleep functions (`Sleep()`, `usleep()`) have ~1-2ms granularity. At sv_fps 125 (8ms frames), a 2ms oversleep is 25% of the frame budget. The `timeResidual` clamping fix makes this mostly unnecessary — only enable if you observe measurable frame timing jitter on specific hardware.

---

### sv_antilagEnable
**Default:** 1 | **Flags:** CVAR_ARCHIVE | **File:** sv_antilag.c (SV_Antilag_Init)

Enable/disable engine-side antilag hit detection.

**Why:** The QVM's built-in antilag only records positions at sv_gameHz (20Hz = 50ms granularity). At sv_fps 60, hit detection needs finer-grained position history. Engine-side antilag records at `sv_physicsScale` sub-ticks per engine tick (~5.5ms granularity at default settings).

**How:** `sv_antilag.c:SV_Antilag_RecordPositions()` called `sv_physicsScale` times per sv_fps tick. On hit detection, rewinds entity positions to `attackTime`, performs trace, restores positions.

---

### sv_physicsScale
**Default:** 3 | **Flags:** CVAR_ARCHIVE | **File:** sv_antilag.c (SV_Antilag_Init)

Number of antilag sub-tick position snapshots per engine tick. At sv_fps 60 with scale 3: records every ~5.5ms. Higher = finer antilag granularity, more memory.

---

### sv_antilagMaxMs
**Default:** 200 | **Flags:** CVAR_ARCHIVE | **File:** sv_antilag.c (SV_Antilag_Init)

Maximum rewind window for antilag (ms). Players with ping above this get no antilag compensation.

---

### sv_bufferMs
**Default:** 0 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Per-client position ring buffer with configurable delay.

- **0** = disabled. No ring buffer, no extra latency. Falls through to direct position correction (sv_extrapolate).
- **-1** = auto. Computes `50 - (1000/sv_fps)` to match vanilla 50ms total latency:
  - sv_fps 125: auto = 42ms buffer (8ms interval + 42ms delay = 50ms total)
  - sv_fps 60: auto = 34ms buffer (16ms + 34ms = 50ms total)
  - sv_fps 20: auto = 0ms (already at 50ms, no delay needed)
- **1-100** = manual delay in milliseconds.

**Why:** At high sv_fps, rapid direction changes produce sawtooth artifacts because each snapshot captures a slightly different velocity vector. The ring buffer provides two smoothing strategies:

**How:** `sv_snapshot.c` maintains a 32-slot ring buffer per client (`svSmoothHistory_t`). Positions are recorded every sv_fps tick in `SV_Frame`. When `sv_bufferMs > 0`, `SV_BuildCommonSnapshot` queries the ring buffer for delayed positions via `SV_SmoothGetPosition()`. Provides the base position for both TR_INTERPOLATE (sv_smoothClients 0) and TR_LINEAR (sv_smoothClients 1) modes — these settings now compose rather than being mutually exclusive.

**Ring buffer capacity:** 32 slots = 256ms at sv_fps 125, 533ms at sv_fps 60. Sufficient for any reasonable buffer depth.

---

### sv_velSmooth
**Default:** 32 | **Flags:** CVAR_ARCHIVE | **File:** sv_init.c

Velocity smoothing window in milliseconds. Averages player velocity over the last N ms from the ring buffer.

- **0** = disabled. Use raw velocity from current tick.
- **1-100** = smoothing window in ms. At sv_fps 60 with sv_velSmooth 32: averages ~2 ticks of velocity data.

**Why:** At high sv_fps, rapid direction changes produce sawtooth artifacts because each snapshot captures a slightly different velocity vector. Averaging over a short window smooths out the transitions.

**How:** Only effective with `sv_smoothClients 1` (TR_LINEAR mode), where cgame uses `trDelta` (velocity) for continuous position evaluation via `BG_EvaluateTrajectory`. The smoothed velocity makes extrapolation between snapshots more stable. Uses the same ring buffer as `sv_bufferMs`. No position latency added — only the velocity vector is smoothed.

**Relationship:** Uses the same ring buffer as `sv_bufferMs`. The ring buffer records whenever either `sv_bufferMs != 0` or `sv_velSmooth > 0`.

---

## Client-Side Cvars

### Removed: cl_snapScaling
`cl_snapScaling` was removed after testing showed it created a serverTime oscillation loop and visible movement stutter at high snapshot rates, especially at 1:1 `sv_fps:sv_snapshotFps`.

Current behavior is back to vanilla client time sync (`+1/-2` drift logic, no extra clamp layer).

For full investigation notes and test matrix, see:
- `docs/debug-session-2026-02-26-cl_snapScaling-stutter.md`

---

## Bug Fixes (no cvar — always active)

These are correctness fixes that should never be disabled:

| Fix | File | What |
|-----|------|------|
| Snapshot dispatch in sv_fps loop | sv_main.c | `SV_IssueNewSnapshot` + `SV_SendClientMessages` inside the `while(timeResidual >= frameMsec)` loop, not after it. Prevents double-tick OS jitter from causing 32ms snapshot gaps. |
| gameTimeResidual preserved | sv_main.c | `sv.gameTimeResidual` NOT reset when sv_fps changes. sv_gameHz doesn't change, so game frame progress is still valid. Resetting caused bot stutter after sv_fps changes. |
| sv_snapshotFps->modified cleared | sv_main.c | Was never cleared in `SV_TrackCvarChanges` → stale flag caused unnecessary recalculation every frame. |
| Listen server SV_BotFrame | sv_main.c | Both dedicated and listen servers now call `SV_BotFrame(sv.gameTime)` inside the sv_gameHz loop. Previously listen servers called with wrong clock. |
| Bot snapshot rate | sv_bot.c | Uses `min(sv_snapshotFps, sv_fps)` instead of raw `sv_fps`. |
| SV_MapRestart_f clock sync | sv_ccmds.c | Syncs `sv.gameTime = sv.time` before `SV_RestartGameProgs()`. Was passing `sv.time` to `GAME_RUN_FRAME` instead of `sv.gameTime`. |
| net_dropsim CVAR_CHEAT | net_ip.c | Changed from `CVAR_TEMP` to `CVAR_CHEAT` to match `cl_packetdelay`. |

---

## Configuration Guide

`sv_bufferMs`, `sv_smoothClients`, and `sv_velSmooth` are independent stages in a pipeline and can be freely combined. Phase 1 resolves the position source (`sv_bufferMs`); Phase 2 resolves the trajectory type (`sv_smoothClients`) and velocity (`sv_velSmooth`).

### Combination Table

| sv_smoothClients | sv_bufferMs | sv_velSmooth | Position Source | Trajectory | Velocity | Description |
|---|---|---|---|---|---|---|
| 0 | 0 | any | Current | TR_INTERPOLATE | Raw | **Default.** Standard extrapolation between game frames. Lowest latency. |
| 0 | -1 | any | Delayed (auto) | TR_INTERPOLATE | Raw | Position delayed to match vanilla 50ms total latency. Stable lerp targets. |
| 0 | 1-100 | any | Delayed (manual) | TR_INTERPOLATE | Raw | Manual position delay in ms. Trades latency for stability. |
| 1 | 0 | 0 | Current | TR_LINEAR | Raw | Continuous trajectory evaluation. No smoothing. |
| 1 | 0 | 1-100 | Current | TR_LINEAR | Smoothed | Continuous trajectory with averaged velocity. Reduces direction-change artifacts. |
| 1 | -1 | 1-100 | Delayed (auto) | TR_LINEAR | Smoothed | **Best of both worlds.** Stable delayed base position + smoothed velocity for extrapolation. |
| 1 | 1-100 | 1-100 | Delayed (manual) | TR_LINEAR | Smoothed | Same as above with manual delay control. |

### Recommended Configurations

- **Competitive (lowest latency):** `sv_smoothClients 0`, `sv_bufferMs 0`, `sv_velSmooth 0` — raw positions, minimal processing
- **Balanced:** `sv_smoothClients 0`, `sv_bufferMs -1`, `sv_velSmooth 0` — position delay matches vanilla feel
- **Smoothest (experimental):** `sv_smoothClients 1`, `sv_bufferMs -1`, `sv_velSmooth 32` — delayed position + smoothed velocity + TR_LINEAR
