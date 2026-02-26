# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Windows (MSVC)
Open `code/win32/msvc2017/quake3e.sln` (or msvc2019) in Visual Studio, compile the `quake3e` project. Output goes to `code/win32/msvc2017/output/`. For Vulkan: clean solution, select `renderervk` dependency instead of `renderer`.

### Linux / MinGW
```
make                              # default build (client + server)
make ARCH=x86_64                  # explicit 64-bit
make BUILD_SERVER=1 BUILD_CLIENT=0  # dedicated server only
make install DESTDIR=<game_dir>   # install to game directory
```

### CMake
```
cmake -B build && cmake --build build
```

Key Makefile toggles: `USE_VULKAN=1`, `USE_SDL=0`, `USE_CURL=1`, `BUILD_CLIENT=1`, `BUILD_SERVER=1`

The engine binary is `quake3e` (client) or `quake3e.ded` (dedicated server). Copy into an existing Q3A/UT installation to run.

---

## Project Summary

Urban Terror 4.3 (UT4.3) competitive server engine enhancement built on **Quake3e**. The project runs `sv_fps 60` for high-rate input sampling while keeping `sv_gameHz 20` for QVM game logic compatibility. All engine changes are C code in `code/server/`. The UT4.3 QVM binaries (qagame, cgame, ui) are closed-source — the UT4.2 source tree at `UrbanTerror42_Source/mod/code/` is reference only. QVM patches are applied via **Ghidra + custom pk3** (binary patching of the 4.3 QVM).

---

## Repository Layout

```
code/server/              ← all engine changes (C, compiled into quake3e binary)
  sv_main.c               ← frame loop, sv_fps/sv_gameHz decoupling, snapshot dispatch
  sv_client.c             ← multi-step Pmove, sv_pmoveMsec clamping, bot exclusion
  sv_init.c               ← cvar registration
  sv_snapshot.c           ← engine-side player position extrapolation
  sv_ccmds.c              ← SV_MapRestart_f fix
  sv_antilag.c            ← engine-side antilag
  sv_antilag.h

UrbanTerror42_Source/mod/code/
  cgame/                  ← cgame reference source (4.2)
    cg_snapshot.c         ← snapshot processing, CG_ProcessSnapshots
    cg_ents.c             ← CG_CalcEntityLerpPositions, CG_InterpolateEntityPosition
    cg_view.c             ← CG_DrawActiveFrame, ut_timenudge applied here
    cg_main.c             ← cvar table, cvarLimitTable (snaps, ut_timenudge limits)
    cg_predict.c          ← client-side prediction
    cg_players.c          ← player rendering, animation (C_SetLerpFrameAnimation)
  game/
    g_active.c            ← antiwarp +50 hardcode, ClientThink_real
    bg_misc.c             ← BG_PlayerStateToEntityState (trDelta = velocity)
```

---

## Engine Work — COMPLETED

### Custom CVars

| Cvar | Default | Description |
|------|---------|-------------|
| `sv_fps` | `60` | Engine tick / input sampling rate |
| `sv_gameHz` | `20` | QVM GAME_RUN_FRAME rate (MUST stay 20 — see constraints) |
| `sv_snapshotFps` | `60` | Snapshot send rate to clients |
| `sv_pmoveMsec` | `8` | Max Pmove physics step (ms) |
| `sv_busyWait` | `0` | Spin last N ms before frame |
| `sv_antilagEnable` | `1` | Engine antilag on/off |
| `sv_physicsScale` | `3` | Antilag sub-ticks per game frame |
| `sv_antilagMaxMs` | `200` | Max rewind window (ms) |

### Frame Loop (sv_main.c)

```
Com_Frame → SV_Frame(msec)
  sv.timeResidual += msec
  clamp timeResidual to frameMsec*2-1   ← prevents burst on fps change

  while timeResidual >= frameMsec (1000/sv_fps):
      timeResidual -= frameMsec
      svs.time += frameMsec
      sv.time  += frameMsec

      [antilag: record sv_physicsScale position snapshots]

      emptyFrame = qtrue   ← per sv_fps tick (USE_MV multiview)
      gameTimeResidual += frameMsec
      while gameTimeResidual >= gameMsec (1000/sv_gameHz):
          gameTimeResidual -= gameMsec
          sv.gameTime += gameMsec
          SV_BotFrame(sv.gameTime)
          GAME_RUN_FRAME(sv.gameTime)
          emptyFrame = qfalse

      SV_IssueNewSnapshot()    ← per sv_fps tick
      SV_SendClientMessages()  ← per sv_fps tick (key fix for netgraph jitter)

  SV_CheckTimeouts()  ← once per Com_Frame
```

### Multi-Step Pmove (sv_client.c)

- Fires multiple QVM GAME_CLIENT_THINK calls per ucmd, each clamped to `sv_pmoveMsec` (8ms)
- Full real delta consumed across all steps → Pmove physics correct, timers correct
- **Critical safety**: If commandTime doesn't advance on first call → `goto single_call` (intermission/pause guard — prevents infinite loop)
- Bots excluded from clamping (they use 50ms steps = level.time)
- Check: `cl->netchan.remoteAddress.type != NA_BOT`

### Engine-Side Position Extrapolation (sv_snapshot.c)

- `BG_PlayerStateToEntityState` only called at sv_gameHz (20Hz) in `ClientEndFrame`
- At sv_fps 60: three consecutive snapshots have identical player positions → stutter
- Fix: in `SV_BuildCommonSnapshot`, forward-extrapolate each player `trBase` by `sv.time - sv.gameTime` ms using `trDelta` (= velocity, confirmed in bg_misc.c:2041)
- Guard: `sv.time > sv.gameTime && es->number < sv_maxclients && es->pos.trType == TR_INTERPOLATE`

### Antilag (sv_antilag.c)

- Engine-side replaces QVM antilag (QVM antilag only records at 20Hz = 50ms granularity — unacceptable at sv_fps 60)
- Records `sv_physicsScale` snapshots per engine tick (~5.5ms granularity at default settings)
- Rewinds entity positions to `attackTime` for hit detection, restores after

### sv_ccmds.c Fix

- `SV_MapRestart_f` was calling `GAME_RUN_FRAME` with `sv.time` instead of `sv.gameTime`
- Fix: sync `sv.gameTime = sv.time`, `sv.gameTimeResidual = 0` before `SV_RestartGameProgs()`, advance both in warmup loops

### Why sv_gameHz Must Stay at 20

`g_active.c` ~L1806: `ent->client->pers.cmd.serverTime += 50` — hardcoded antiwarp blank command injection. At sv_gameHz > 20 this teleports lagging players. Engine-side workaround impossible without QVM source.

### Snapshot Dispatch

`SV_IssueNewSnapshot` + `SV_SendClientMessages` moved **inside** the sv_fps while loop. Previously called once per `Com_Frame` — OS sleep overshoot caused double-tick scenarios with only one snapshot dispatch, producing 32ms netgraph spikes. Each engine tick now dispatches independently.

### Client snaps Cvar

Engine ignores client `snaps` userinfo — `sv_snapshotFps` is fully authoritative (`SV_UserinfoChanged` in sv_client.c).

---

## cgame Work — IN PROGRESS

**Goal:** Improve cgame tolerance for jittery/high-latency connections and prevent breakage at elevated sv_fps rates.

**Patching method:** Ghidra analysis of UT4.3 cgame QVM binary + custom pk3 with replacement functions.

### cgame Frame Flow (from 4.2 source reference)

```
CG_DRAW_ACTIVE_FRAME(serverTime, stereoView, demoPlayback)
  → CG_DrawActiveFrame() in cg_view.c
      cg.time = serverTime          ← engine sets this
      ...
      cg.time -= ut_timenudge.integer   ← UT-specific lag compensation
      CG_ProcessSnapshots()         ← advances cg.snap / cg.nextSnap
      CG_PredictPlayerState()
      CG_AddPacketEntities()
        → CG_CalcEntityLerpPositions() per entity
            → CG_InterpolateEntityPosition() if interpolating
      ...
      cg.frametime = cg.time - cg.oldTime
```

### Key cvars (from cg_main.c 4.2 source)

| Cvar | Default | Limit min | Limit max | Locked | Notes |
|------|---------|-----------|-----------|--------|-------|
| `ut_timenudge` | 25 (table) / 0 (limit table default) | 0 | 50 | No | USERINFO, ARCHIVE — applied as `cg.time -= ut_timenudge` before snapshot processing |
| `snaps` | 20 | 20 | 125 | Yes (byte=1) | Server bypasses via engine, but QVM still enforces min=20 |
| `cl_timeNudge` | 0 | 0 | 0 | Yes (byte=1) | Forced to 0 by UT — standard Q3 time nudge disabled |
| `cg_smoothClients` | 0 | — | — | No | When 0: forces TR_INTERPOLATE on all player entities, disabling any extrapolation |

### Identified Problems

#### Problem 1: `frameInterpolation` not clamped — overshoot causes visual errors

From `cg_ents.c`:
```c
// set cg.frameInterpolation
delta = cg.nextSnap->serverTime - cg.snap->serverTime;  // ~16ms at sv_fps 60
cg.frameInterpolation = (float)(cg.time - cg.snap->serverTime) / delta;
```

At sv_fps 60, the 16ms window is tight. If `cg.time` advances past `cg.nextSnap->serverTime` before `CG_TransitionSnapshot` fires (any rendering frame where the snapshot arrives late), `frameInterpolation > 1.0`. The lerp overshoots — entity appears ahead of its actual position, then snaps back when the snapshot is processed. No clamping exists in the code.

Fix: clamp `cg.frameInterpolation` to `[0.0, 1.2]` (small overshoot allowance) or `[0.0, 1.0]`.

#### Problem 2: No extrapolation when `nextSnap` is NULL

From `cg_ents.c` `CG_CalcEntityLerpPositions`:
```c
if (cent->interpolate && (cent->currentState.pos.trType == TR_INTERPOLATE)) {
    CG_InterpolateEntityPosition(cent);  // requires cg.nextSnap != NULL — crashes if null
    return;
}
// fallthrough: BG_EvaluateTrajectory(&cent->currentState.pos, cg.time, ...)
```

And at the frame level:
```c
if (cg.nextSnap) { ... cg.frameInterpolation = ...; }
else { cg.frameInterpolation = 0; }  // freeze — no extrapolation
```

When `cg.nextSnap` is NULL (packet not yet arrived), positions freeze at `cg.snap` values. At sv_fps 60, any 16ms packet delay produces a visible freeze frame. At sv_fps 20 the same delay was within one frame period and less noticeable.

Fix: when `nextSnap` is NULL, extrapolate using velocity from `trDelta` (same logic as the engine-side fix in sv_snapshot.c). `BG_EvaluateTrajectory` already handles TR_LINEAR_STOP correctly — the issue is TR_INTERPOLATE entities have `trDelta = velocity` but no time advancement in the trajectory. Need to manually apply: `lerpOrigin += trDelta * (cg.time - snap->serverTime) * 0.001`.

#### Problem 3: Bot animation timing (known limitation — unfixable without cgame source)

`C_SetLerpFrameAnimation` in cg_players.c sets `lerpFrame->animationTime = cg.time` when animation changes. At 60Hz snapshots / 20Hz bot updates, `cg.time` when the new animation state arrives is up to 33ms ahead of when it actually started server-side → bot jump/land animations play from wrong positions. Real players unaffected. Requires cgame QVM source to fix properly.

### Patching Priorities (cgame)

1. **`frameInterpolation` clamp** — locate computation in 4.3 binary, patch to clamp [0.0, 1.0]. Low risk, high reward. (`cg_ents.c` ~L1351 in 4.2 source)

2. **Extrapolation fallback when nextSnap is NULL** — modify `CG_CalcEntityLerpPositions` (or its equivalent in 4.3) to forward-extrapolate using `trDelta * dt` when `nextSnap` is NULL and entity is a player (number < MAX_CLIENTS). (`cg_ents.c` ~L1080-1119 in 4.2)

### cgame Snapshot Processing Flow

```
CG_ProcessSnapshots():
  trap_GetCurrentSnapshotNumber(&n, &cg.latestSnapshotTime)

  do:
    if !cg.nextSnap:
      snap = CG_ReadNextSnapshot()
      if !snap: break  ← extrapolation territory (nextSnap = NULL)
      CG_SetNextSnap(snap)

    if cg.time >= cg.snap->serverTime && cg.time < cg.nextSnap->serverTime:
      break  ← ideal interpolating state

    CG_TransitionSnapshot()  ← advance snap → nextSnap
  while 1

  // Guard: if cg.time < cg.snap->serverTime, set cg.time = cg.snap->serverTime
```

The `break` condition requires `cg.time < cg.nextSnap->serverTime`. At sv_fps 60, this 16ms window closes quickly. If cg.time runs fast (high render fps, no vsync), it can overshoot and CG_TransitionSnapshot fires prematurely — consuming nextSnap before the render frame uses it.

---

## Known Issues Status

| Issue | Status |
|-------|--------|
| End of round freeze | **FIXED** (confirmed live) |
| rcon map load broken | Likely fixed (same root cause) — needs confirmation |
| Bleed/bandage timing | **FIXED** (confirmed live) |
| Fast strafe visual stutter | **FIXED** (confirmed live) |
| Bot movement | Shelved |
| cgame jitter tolerance | **IN PROGRESS** |

---

## Testing Priorities

1. rcon `map <mapname>` during live game (likely fixed, needs confirmation)
2. cgame jitter: simulate packet loss, measure freeze duration

---

## Files Modified (engine)

| File | Summary |
|------|---------|
| `sv_main.c` | Dual-rate loop, antilag recording, snapshot dispatch inside sv_fps loop, emptyFrame reset per tick |
| `sv_init.c` | Register sv_gameHz, sv_snapshotFps, sv_busyWait, sv_pmoveMsec, antilag cvars |
| `sv_client.c` | Multi-step Pmove with stall detection, bot exclusion, snaps cvar bypass |
| `sv_snapshot.c` | Engine-side player position extrapolation using sv.time-sv.gameTime offset, extrapolation cap |
| `sv_ccmds.c` | SV_MapRestart_f: sv.gameTime sync, correct clock passed to GAME_RUN_FRAME |
| `sv_antilag.c` | Full engine-side antilag implementation |
| `sv_antilag.h` | Antilag interface |

## Files Modified (client — jitter tolerance)

| File | Summary |
|------|---------|
| `client.h` | Added `snapshotMsec` field to `clientActive_t` |
| `cl_parse.c` | Snapshot interval measurement (EMA), pre-compute from `sv_snapshotFps` configstring |
| `cl_cgame.c` | Scaled RESET_TIME/fast-adjust thresholds, scaled extrapolation detection, scaled drift correction, serverTime clamp to prevent frameInterpolation overshoot, timedemo fix |
| `cl_input.c` | Download throttle scaled to snapshot interval |
| `net_ip.c` | `net_dropsim` flagged as `CVAR_CHEAT` |

### Jitter Tolerance System (cl_cgame.c)

**Snapshot interval tracking:** `cl.snapshotMsec` is measured from actual snapshot arrivals using exponential moving average (75% old / 25% new). Pre-initialized from `sv_snapshotFps` in server configstring on connect. Clamped [8, 100].

**Scaled time sync thresholds:**
- RESET_TIME: `snapshotMsec * 10` (min 200ms) — hard reset on large desync
- Fast adjust: `snapshotMsec * 2` (min 50ms) — halve the difference
- Extrapolation detection: `snapshotMsec / 3` (clamped [3, 16]) — flag for drift correction
- Drift pullback: `-4ms/frame` at high rates (snapshotMsec < 30), `-2ms/frame` at vanilla rates

**serverTime clamp:** `cl.serverTime` capped at `cl.snap.serverTime + cl.snapshotMsec`. Prevents cgame from computing `frameInterpolation > 1.0` which causes position overshoot and snap-back.

### Antiwarp Analysis

g_antiwarp is safe at any sv_fps with sv_gameHz 20:
- `G_RunClient` fires at 20Hz (game frame rate)
- `lastCmdTime` tracks `level.time` via `ClientThink` calls
- At sv_fps 60: 3x commands per game frame → `lastCmdTime` always current
- Next game frame: `level.time - lastCmdTime = 50`, `50 > g_antiWarpTol(50)` = false → no trigger
- Only triggers on genuine packet loss (100ms+ gap) → correct behavior
- The hardcoded `serverTime += 50` matches game frame duration → safe injection
