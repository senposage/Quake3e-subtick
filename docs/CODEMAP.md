# Code Map — Quake3e-subtick

Quick reference for finding key code paths. All custom changes are in `code/server/`, `code/client/`, and documentation files.

---

## Server Core

### Frame Loop
- **`code/server/sv_main.c`** — `SV_Frame()`: outer sv_fps loop, inner sv_gameHz loop, snapshot dispatch, SV_BotFrame placement
- **`code/server/sv_init.c`** — All custom cvar registration (sv_fps, sv_gameHz, sv_snapshotFps, sv_pmoveMsec, sv_extrapolate, sv_smoothClients, sv_bufferMs, sv_velSmooth, sv_busyWait)

### Client Think / Physics
- **`code/server/sv_client.c`** — `SV_ClientThink()`: sv_pmoveMsec multi-step loop, commandTime stall detection (`goto single_call`), bot exclusion
- **QVM intercept (server-side):** `sv_game.c` → `case G_CVAR_SET` — blocks QVM from setting `sv_fps`, `g_failedvotetime`

### Snapshots & Visual Smoothing
- **`code/server/sv_snapshot.c`** — `SV_BuildCommonSnapshot()`: sv_extrapolate position correction, sv_smoothClients TR_LINEAR injection, sv_bufferMs ring buffer, sv_velSmooth velocity averaging, dead-zone checks
- **`SV_SmoothRecord()`** — ring buffer recording (called from SV_Frame)
- **`SV_SmoothGetPosition()`** — ring buffer interpolation query
- **Ring buffer struct:** `svSmoothHistory_t` — 32-slot per-client history

### Antilag
- **`code/server/sv_antilag.c`** — `SV_Antilag_RecordPositions()`, position rewind/restore for hit detection
- **`code/server/sv_antilag.h`** — Interface

### Map Management
- **`code/server/sv_ccmds.c`** — `SV_MapRestart_f()`: gameTime sync before restart, GAME_RUN_FRAME uses sv.gameTime

---

## Client Core

### Time Sync
- **`code/client/cl_cgame.c`** — `CL_AdjustTimeDelta()`: vanilla +1/-2 drift, RESET_TIME, fast adjust thresholds
- **`code/client/cl_cgame.c`** — `CL_SetCGameTime()`: serverTime advancement, extrapolation detection
- **QVM intercept (client-side):** `cl_cgame.c` → `case CG_CVAR_SET` — blocks QVM from setting `snaps`, `cg_smoothClients`

### Snapshot Parsing
- **`code/client/cl_parse.c`** — Snapshot interval measurement, sv_snapshotFps configstring

### Input
- **`code/client/cl_input.c`** — cl_maxpackets, download throttle

---

## QVM Reference (4.2 source — read only)

These files are in `UrbanTerror42_Source/` and are reference only. We cannot modify the 4.3 QVM.

### Game Module
- **`g_active.c`** — `ClientThink_real()`: Pmove call, `ClientTimerActions()`, antiwarp (`serverTime += 50`)
- **`g_main.c`** — `G_RunFrame()`, `ExitLevel()`, `FRAMETIME` usage

### Shared Physics
- **`bg_pmove.c`** — `Pmove()`: all physics (movement, jump, crouch, stamina, weaponTime, recoil). Everything is `pml.msec`-based.
- **`bg_misc.c`** — `BG_PlayerStateToEntityState()`: copies `ps->velocity` → `s->pos.trDelta`, sets `TR_INTERPOLATE`. `BG_EvaluateTrajectory()`: switch on trType (TR_INTERPOLATE just does VectorCopy — the Ghidra patch target).

### cgame Rendering
- **`cg_ents.c`** — `CG_AddPacketEntities()`: frameInterpolation computation (unclamped — Ghidra Patch 2). `CG_CalcEntityLerpPositions()`: interpolate vs extrapolate decision, nextSnap null check (Ghidra Patch 3). `cg_smoothClients` TR_INTERPOLATE force.
- **`cg_local.h`** — `cg_t` struct: `frameInterpolation`, `snap`, `nextSnap`, `time`

---

## Documentation

- **`CLAUDE_CONTEXT.md`** — Full project context, architecture, all custom changes explained
- **`CLAUDE.md`** — Claude Code project instructions
- **`CVARS.md`** — All custom cvars with defaults, flags, rationale, and interaction table
- **`docs/ghidra-cgame-patches.md`** — Three QVM binary patches for cgame (frameInterpolation clamp, TR_INTERPOLATE velocity extrapolation, nextSnap null fallback)
- **`docs/debug-session-2026-02-26-cl_snapScaling-stutter.md`** — cl_snapScaling oscillation investigation and removal
- **`docs/debug-session-2026-02-27-smoothing-jitter.md`** — sv_smoothClients/sv_bufferMs stationary jitter investigation

---

## Key Data Structures

### Server
- `server_t` (`sv`): `time`, `gameTime`, `gameTimeResidual`, `timeResidual`
- `serverStatic_t` (`svs`): `time`, `emptyFrame`
- `client_t` (`cl`/`svs.clients[]`): `snapshotMsec`, `lastUsercmd`, `netchan`
- `svSmoothHistory_t`: per-client ring buffer, 32 slots, records origin/velocity/time

### Client
- `clientActive_t` (`cl`): `serverTime`, `serverTimeDelta`, `snap`, `nextSnap`, `snapshotMsec`
- `clSnapshot_t`: `serverTime`, `ps` (playerState), `entities[]`

### Shared
- `trajectory_t`: `trType`, `trTime`, `trDuration`, `trBase[3]`, `trDelta[3]`
- `entityState_t`: `pos` (trajectory_t), `apos` (trajectory_t), `number`
- `playerState_t`: `origin[3]`, `velocity[3]`, `commandTime`

---

## Key Constants

| Constant | Value | Where |
|----------|-------|-------|
| TR_STATIONARY | 0 | bg_public.h |
| TR_INTERPOLATE | 1 | bg_public.h |
| TR_LINEAR | 2 | bg_public.h |
| TR_LINEAR_STOP | 3 | bg_public.h |
| Dead-zone threshold | 100.0f | sv_snapshot.c (DotProduct check on velocity², threshold 100.0f = 10 ups magnitude) |
| Ring buffer size | 32 | sv_snapshot.c |
| FRAMETIME (QVM) | 100 | g_local.h (only nextthink, self-correcting) |
| Antiwarp inject | 50ms | g_active.c (hardcoded, breaks at sv_gameHz != 20) |
