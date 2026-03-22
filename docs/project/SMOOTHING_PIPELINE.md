# Entity Smoothing Pipeline

> How `sv_smoothClients`, `cl_adaptiveTiming`, and the UrT 4.3 QVM patches
> work together to deliver smooth, jitter-free player movement at any `sv_fps`.

---

## Table of Contents

1. [Background and Problem Statement](#background-and-problem-statement)
2. [The Three Layers](#the-three-layers)
3. [Layer 1 -- Server: sv_smoothClients](#layer-1----server-sv_smoothclients)
4. [Layer 2 -- Client: cl_adaptiveTiming](#layer-2----client-cl_adaptivetiming)
5. [Layer 3 -- QVM: Runtime Patches](#layer-3----qvm-runtime-patches)
6. [How They Work Together](#how-they-work-together)
7. [Failure Modes and Fixes](#failure-modes-and-fixes)
8. [Vanilla Server Behavior](#vanilla-server-behavior)
9. [Cvar Reference](#cvar-reference)

---

## Background and Problem Statement

UrbanTerror 4.3 ships a single `cgame.qvm` binary that was designed around a
20 Hz snapshot rate (`sv_fps 20`).  Running the game at 60 Hz or 125 Hz exposes
three latent bugs in the QVM's entity position code:

1. **Jitter at high sv_fps** -- `frameInterpolation` oscillates because the QVM
   forces every player entity onto the pure position-lerp path
   (`TR_INTERPOLATE`), making rendering a function of snap-transition timing
   rather than real time.  At 125 Hz the visible position error is ~2.4 units
   per frame at 300 ups.

2. **Crash on first rendered frame / lag spike** -- `CG_InterpolateEntityPosition`
   calls `CG_Error()` (fatal drop) when `cg.nextSnap == NULL`.  This fires on
   the very first frame after loading any map, making the game unplayable
   without a fix.

3. **Broken TR_INTERPOLATE extrapolation** -- `BG_EvaluateTrajectory` routes
   `TR_INTERPOLATE` to the `TR_STATIONARY` handler (VectorCopy only), so
   velocity data in snapshots is silently discarded during the inter-snapshot
   window.

These bugs require co-operation between three engine-side layers to fix
completely without breaking vanilla server compatibility.

---

## The Three Layers

```
+------------------------------------------------------+
|  SERVER (sv_snapshot.c)                              |
|  sv_smoothClients  sv_velSmooth  sv_bufferMs         |
|  -- builds TR_LINEAR snapshots with trTime anchor    |
+------------------------------------------------------+
            |  network  |  packets
+------------------------------------------------------+
|  CLIENT  (cl_cgame.c / cl_parse.c)                   |
|  cl_adaptiveTiming                                   |
|  -- keeps cl.serverTime proportional to snapshotMsec |
+------------------------------------------------------+
            |  VM_Call  |  CG_DRAW_ACTIVE_FRAME
+------------------------------------------------------+
|  QVM PATCHES (vm.c  VM_URT43_CgamePatches)           |
|  Patch 2  Patch 3  Patch 1 + cg_smoothClients force  |
|  -- fix QVM bugs in the official cgame.qvm binary    |
+------------------------------------------------------+
```

All three layers must be active on a custom server.  On a vanilla server only
Patches 2 and 3 are applied (safe on any server); Patch 1 and sv_smoothClients
require server-side `trTime` anchoring.

---

## Layer 1 -- Server: sv_smoothClients

### What the QVM puts in snapshots

`BG_PlayerStateToEntityState` (inside the game QVM) converts each player's
`playerState_t` to an `entityState_t` for the snapshot.  It always writes:

```c
es->pos.trType = TR_INTERPOLATE;
es->pos.trBase = ps->origin;    // position at this game frame
es->pos.trDelta = ps->velocity; // ignored by client without sv_smoothClients
es->pos.trTime  = 0;            // NEVER set for TR_INTERPOLATE entities
```

`trTime` stays at 0 forever because the QVM only writes it for
`TR_LINEAR`/`TR_GRAVITY` entities.  The client-side `BG_EvaluateTrajectory`
formula for TR_LINEAR is:

```
result = trBase + trDelta * (evalTime - trTime) / 1000
```

With `trTime == 0` that `dt` is the full game clock (~millions of ms), teleporting
every entity to a nonsense position on every render frame.

### What sv_smoothClients does

`sv_snapshot.c` runs after each `sv_fps` tick and patches player entity
trajectories before they are serialised into the snapshot buffer.  For each
client entity (`es->number < sv.maxclients`) that still carries
`trType == TR_INTERPOLATE`:

**Phase 1 -- position source:**

| Source | Condition |
|---|---|
| `sv_bufferMs` ring buffer (delayed position) | `sv_bufferMs != 0` and not a bot |
| `ps->origin + ps->velocity * dt` (forward-extrapolation to sv.time) | `sv_gamehz > 0` and player packets lagged |
| `ps->origin` / `ps->velocity` (current game state) | default |

**Phase 2 -- trajectory type:**

If the entity is moving (`|velocity| > 10 ups`):

```c
es->pos.trType  = TR_LINEAR;
es->pos.trBase  = resolved_origin;
es->pos.trDelta = resolved_velocity;   // optionally EMA-smoothed via sv_velSmooth
es->pos.trTime  = sv.time;             // << the critical anchor
```

If idle (`|velocity| <= 10 ups`):

```c
es->pos.trType  = TR_INTERPOLATE;      // keep interpolation for stationary players
es->pos.trBase  = resolved_origin;
es->pos.trTime  = sv.time;             // anchored regardless
```

**trTime anchor (always, even without sv_smoothClients):**

Even if `sv_smoothClients` and `sv_extrapolate` are both off, the engine still
anchors `trTime = sv.time` for all `TR_INTERPOLATE` client entities (lines
958-971 of `sv_snapshot.c`).  This is required by QVM Patch 1 which activates
the TR_LINEAR formula for those entities -- without the anchor the dt is
enormous and entities teleport.

**Origin sync:**

`es->origin` is synced to `es->pos.trBase` so that QVM code that reads
`cent->currentState.origin` directly (e.g. the spot-shadow raytrace) sees the
same extrapolated position as the trajectory, preventing the shadow from
appearing in front of or behind a moving player.

### sv_velSmooth

When `sv_velSmooth > 0`, the per-client position ring buffer (which also records
velocity each tick) is queried for an exponentially-smoothed average velocity
over the last `sv_velSmooth` ms.  This eliminates the 50/50 velocity flip that
occurs at direction-change frames (one tick says +300, next says -300) and
gives the client a smooth, continuous `trDelta` to extrapolate.

### sv_bufferMs

When `sv_bufferMs > 0`, positions are read from the ring buffer at
`sv.time - bufferMs` instead of `sv.time`.  This trades a small constant
visual delay for greatly improved smoothness by giving the server a window in
which to gather multiple position samples before committing the snapshot
trajectory.  The `trTime` anchor is adjusted accordingly:
`es->pos.trTime = sv.time - bufferMs`.

---

## Layer 2 -- Client: cl_adaptiveTiming

### The vanilla timing problem at high sv_fps

The stock Q3 timing loop uses hard-coded thresholds:

- **RESET** (snap to new delta): `|deltaDelta| > 500 ms`
- **FAST** (average new delta): `|deltaDelta| > 100 ms`
- **Slow drift**: +/-1 ms per snapshot (toward target delta)

These thresholds were designed for `sv_fps 20` (50 ms snapshot interval).  At
60 Hz (16 ms interval) or 125 Hz (8 ms interval):

- The slow-drift rate is `1 ms per 16 ms snapshot = 6.25%`.  A momentary
  300 ms OS hitch takes 300 * 16 / 1 = **4800 snapshots = 80 seconds** to
  correct via slow drift alone.
- The FAST threshold of 100 ms is 6x a single snapshot interval.  Normal
  network jitter (+-5 ms per snapshot) regularly exceeds it, triggering
  frequent hard-averages that over-correct `serverTimeDelta`.
- After an over-correction, `cl.serverTime` can drop below the server's
  `commandTime`, causing every usercmd to be rejected (skipped) by the server:
  the client appears to freeze with `ping=999` until the slow drift corrects
  over several seconds.

### What cl_adaptiveTiming does

When `cl_adaptiveTiming 1` and the server sends `sv_snapshotFps` in its
serverinfo (custom server detection), the thresholds scale with
`cl.snapshotMsec`:

```c
resetTime  = max(snapshotMsec * 10, 500)  // e.g. 160 ms at 60Hz, 500 ms floor
fastAdjust = max(snapshotMsec *  2,  50)  // e.g.  32 ms at 60Hz,  50 ms floor
```

At 20 Hz (`snapshotMsec = 50`): `resetTime = 500`, `fastAdjust = 100` --
identical to vanilla.  At 60 Hz: `resetTime = 160`, `fastAdjust = 32`.  At
125 Hz: `resetTime = 80`, `fastAdjust = 16`.

The slow-drift backward step is also scaled:

```c
slowFrac -= (snapshotMsec < 30) ? 2 : 4;  // +/-0.5 ms/snap at 60Hz, 1 ms/snap at 20Hz
```

This keeps the drift rate proportional to the snapshot interval, so recovery
time is constant in wall-clock ms regardless of sv_fps.

### commandTime floor

On custom servers, if `cl.serverTime` falls at or below `snap.ps.commandTime`
(meaning every outgoing usercmd would be rejected), both `cl.serverTime` and
`cl.serverTimeDelta` are immediately snapped up to `commandTime + 1`.  This
converts a multi-second slow-drift recovery into an instant correction,
eliminating the `ping=999` / dropped-command symptom after timing hiccups.

### Extrapolated-snapshot detection

`cl.extrapolatedSnapshot` is set when the client's render time has advanced
past the latest received snapshot.  `cl_adaptiveTiming` scales the detection
threshold:

```c
extrapolateThresh = clamp(snapshotMsec / 3, 3, 16) ms
```

At 125 Hz (8 ms snaps): threshold = 3 ms.  At 20 Hz: 16 ms.  The tighter
threshold at high sv_fps ensures the timing loop pulls back sooner, reducing
how often the QVM sees `cg.nextSnap == NULL`.

---

## Layer 3 -- QVM: Runtime Patches

These patches are applied in-memory to the official UrT 4.3 `cgame.qvm`
(CRC32 `0x1289DB6B`) when it is loaded by the engine.  The QVM binary is
never modified on disk.

See `docs/project/QVM.md` for full instruction-level detail.

### Patch 2 (bit 0): frameInterpolation clamp

The QVM clamps `frameInterpolation` with:

```
lower threshold: 0.1f  (should be 0.0f)
upper clamp:    ~0.99f  (should be 1.0f)
```

At `sv_fps 60` with a 16 ms snap interval and a 16 ms render frame, a client
that receives exactly one snapshot per frame gets `frameInterpolation = 1.0`.
The broken upper clamp discards this and snaps to ~0.99, creating a 0.16 ms
position error every frame.  More visibly, the broken lower threshold discards
small-but-valid `frameInterpolation` values near 0, producing a sawtooth
position discontinuity at every snap transition.

Fixed by patching two constant values at instruction indices `0xa688` and
`0xa692`.  Safe on any server.

### Patch 3 (bit 1): CG_InterpolateEntityPosition nextSnap NULL crash

`CG_InterpolateEntityPosition` dereferences `cg.nextSnap` without a NULL
check, then calls `CG_Error()` (fatal drop) if it is NULL.  This crashes on:

- The **first rendered frame** after any map load -- only one snapshot exists
  when the QVM calls `CG_DRAW_ACTIVE_FRAME` for the first time.
- Any **lag spike** that exhausts the snapshot buffer -- `CL_GetSnapshot`
  returns `qfalse` for the "next" sequence number, leaving `cg.nextSnap = NULL`.
- The **first frame after map_restart** -- same as first-load scenario.

Fixed by replacing the 5-instruction `CG_Error` path with `CONST 0x1594d +
JUMP` to the function's `PUSH + LEAVE` (early return).  When `cg.nextSnap == NULL`
the function simply returns without updating the entity position.

**Interaction with Patch 1:** With `cg_smoothClients=1` forced by Patch 1,
player entities carry `TR_LINEAR` and are evaluated via
`BG_EvaluateTrajectory(cg_time)` -- they never enter `CG_InterpolateEntityPosition`
at all during extrapolation.  This means player entities **keep moving** on
velocity even when `cg.nextSnap == NULL`, while Patch 3 cleanly handles
non-player and idle entities that use the position-lerp path.

Safe on any server.  **Required** -- without this patch the game crashes
every time a map loads.

### Patch 1 (bit 2): TR_INTERPOLATE velocity extrapolation + cg_smoothClients

**Problem A -- BG_EvaluateTrajectory fallthrough:**

The QVM switch-table entry for `trType == TR_INTERPOLATE` (data address
`0xdbb4`) points to the `TR_STATIONARY` handler (`VectorCopy` only) instead of
`TR_LINEAR`.  Velocity data in `trDelta` is silently discarded.

Fixed by inserting a guard stub (instr `0x3b434`) as the switch target:

```
if (trTime == 0):   fall through to TR_STATIONARY (VectorCopy)
else:               jump to TR_LINEAR handler (trBase + trDelta * dt / 1000)
```

The `trTime == 0` branch handles the predicted-player entity
(`BG_PlayerStateToEntityState` never sets `trTime` for the local player), so
the guard never misfires for the predicted self.

**Problem B -- CG_CalcEntityLerpPositions trType override (root cause of jitter):**

`CG_CalcEntityLerpPositions` (instr `0x1594f`) runs every render frame and
unconditionally writes `trType = TR_INTERPOLATE` into both `currentState` and
`nextState` for every entity with `number < 64` when `cg_smoothClients == 0`
(the QVM default).

This discards the `TR_LINEAR` trajectory type and `trDelta` velocity that the
server set via `sv_smoothClients`, routing all player entities through
`CG_InterpolateEntityPosition`.  That function evaluates `BG_EvaluateTrajectory`
at `snap->serverTime`, giving:

```
dt = snap->serverTime - trTime = snap->serverTime - sv.time = 0
result = trBase + trDelta * 0 = trBase   -- velocity ignored
```

Entity position becomes a pure position-lerp between two snapshot `trBase`
values weighted by `frameInterpolation`.  At 125 Hz (8 ms snaps) a 60 fps
client consumes either 1 or 2 snap transitions per render frame depending on
OS scheduling jitter (+-1-3 ms is normal).  `frameInterpolation` alternates
between ~0.75 and ~1.0 each frame, causing the entity to jump by roughly
`velocity * 8 ms = 2.4 units/frame` at 300 ups -- the visible jitter.

**Fix:** After Patch 1 is applied on a custom server (`!isVanilla`), force
`cg_smoothClients = 1`.  With that value the trType override is skipped;
TR_LINEAR entities use `BG_EvaluateTrajectory(cg_time)`:

```
result = trBase + trDelta * (cg_time - trTime) / 1000
```

This is continuous velocity-based extrapolation evaluated at the actual render
time, completely independent of how many snap transitions occurred since the
last frame.

The `CG_CVAR_SET` intercept in `cl_cgame.c` (line 571) prevents the QVM from
resetting `cg_smoothClients` mid-session.

**Requires server-side `trTime` anchor** -- see Layer 1.  Auto-suppressed on
vanilla servers via `cl_qvmPatchVanilla` (default `3` excludes bit 2).

---

## How They Work Together

The full render-frame pipeline for a moving player entity at 125 Hz (`sv_fps
125`):

```
SERVER TICK (every 8 ms)
  G_RunClient / GAME_CLIENT_THINK -> ps->origin updated
  sv_snapshot.c:
    es->pos.trType  = TR_LINEAR
    es->pos.trBase  = ring-buffer position (sv_bufferMs ago, or current)
    es->pos.trDelta = EMA-smoothed velocity (sv_velSmooth)
    es->pos.trTime  = sv.time
    es->origin      = es->pos.trBase  (shadow-trace sync)
  Snapshot serialised and sent

NETWORK TRANSIT (~25-100 ms)

CLIENT RECEIVES SNAPSHOT
  cl_parse.c: cl.snapshotMsec measured = 8 ms
  cl_adaptiveTiming: resetTime=80ms, fastAdjust=16ms, drift=0.5ms/snap
  cl.serverTime tracked close to snap.serverTime

RENDER FRAME (every ~16 ms at 60 fps)
  CL_RunFrame -> CG_DRAW_ACTIVE_FRAME(cl.serverTime, ...)
  QVM CG_ProcessSnapshots -> cl.snap updated
  QVM CG_CalcEntityLerpPositions:
    cg_smoothClients=1 -> trType override skipped
    entity keeps TR_LINEAR from snapshot
  QVM BG_EvaluateTrajectory(entity, cg.time):
    dt = cg.time - trTime = (cl.serverTime) - (sv.time_when_snapped)
    result = trBase + trDelta * dt / 1000
    -> position is continuous, evaluated at render time
    -> identical result whether this frame consumed 1 or 2 snap transitions
```

**What `cg.nextSnap == NULL` looks like in this pipeline:**

During a lag spike or on the first frame, no "next" snapshot exists.
`CG_ProcessSnapshots` sets `cg.nextSnap = NULL`.

- Player entities (TR_LINEAR): unaffected -- `BG_EvaluateTrajectory(cg.time)` runs
  without referencing nextSnap.  Players continue extrapolating on velocity.
- Other entities (TR_INTERPOLATE idle): Patch 3 makes `CG_InterpolateEntityPosition`
  return early.  Entities hold last known position until the next snapshot arrives.

Without any of the three layers:

| Missing layer | Symptom |
|---|---|
| No Patch 3 | Crash (CG_Error) on first frame of every map load |
| No Patch 2 | Sawtooth position discontinuity at every snap boundary |
| No Patch 1 | TR_INTERPOLATE->TR_STATIONARY: velocity ignored, pure lerp only |
| No cg_smoothClients=1 | Patch 1 is a no-op: QVM overwrites TR_LINEAR back to TR_INTERPOLATE |
| No sv_smoothClients | Snapshots carry TR_INTERPOLATE with trTime=0: Patch 1 produces enormous dt, entities teleport |
| No trTime anchor | Same as no sv_smoothClients even with sv_smoothClients=1 (anchor happens independently) |
| No cl_adaptiveTiming | Timing loop over/under-shoots at high sv_fps; commandTime floor miss causes ping=999 spikes |

---

## Failure Modes and Fixes

### FM-1: Entity teleports every frame (enormous dt)

**Cause:** `trTime == 0`.  The TR_LINEAR formula `trBase + trDelta * dt / 1000`
with `dt = cg.time - 0 = ~1 000 000 ms` produces a position millions of units
from the origin.

**Path:** Occurs when Patch 1 is active but the server has not anchored
`trTime`.  Vanilla servers never write `trTime` for `TR_INTERPOLATE` entities.

**Fix:** `sv_snapshot.c` anchors `trTime = sv.time` unconditionally for all
`TR_INTERPOLATE` client entities, even when `sv_smoothClients = 0`.  Patch 1 is
automatically suppressed on vanilla servers via `cl_qvmPatchVanilla` (bit 2 not
set in default `3`).  The `!isVanilla` gate in `VM_URT43_CgamePatches` also
prevents `cg_smoothClients=1` from being forced on vanilla servers.

### FM-2: Crash on map load / after lag spike

**Cause:** `cg.nextSnap == NULL` -> `CG_InterpolateEntityPosition` -> `CG_Error`.

**Path:** Patch 3 not applied, or not in the patch bitmask.

**Fix:** Patch 3 (bit 1) replaces the `CG_Error` path with an early return.
Default bitmasks (`cl_urt43cgPatches 7` and `cl_qvmPatchVanilla 3`) both include
bit 1, so it is active on all server types.

### FM-3: Player jitter at sv_fps > 60 (~2 units/frame at 300 ups)

**Cause:** `cg_smoothClients = 0` (QVM default).  `CG_CalcEntityLerpPositions`
forces `trType = TR_INTERPOLATE` for all player entities.  At 125 Hz, the number
of snap transitions per 16 ms render frame oscillates between 1 and 2 due to OS
scheduling jitter.  `frameInterpolation` oscillates correspondingly, causing
position to jump by `velocity * snapshotMsec` on every frame where two
transitions occur.

**Fix:** `VM_URT43_CgamePatches` forces `cg_smoothClients = 1` after applying
Patch 1 on custom servers.  The `CG_CVAR_SET` intercept prevents the QVM from
resetting it.

### FM-4: Sawtooth position discontinuity at snap boundaries (Patch 2)

**Cause:** QVM clamps `frameInterpolation` in `[0.1, ~0.99]` instead of `[0.0, 1.0]`.
Small fractions near 0 (start of new snap window) are discarded; values near
1.0 (end of snap window) are clamped down, leaving a constant ~1% position
error at every transition.

**Fix:** Patch 2 (bit 0) corrects the two float constants at instrs `0xa688`
and `0xa692`.

### FM-5: cl.serverTime drifts below commandTime at high sv_fps -> ping=999

**Cause:** A FAST timing adjustment (triggered by normal network jitter, which
is more frequent relative to snapshot interval at high sv_fps) over-averages
`serverTimeDelta` by too much.  `cl.serverTime` drops below the server's
`commandTime`.  Every outgoing usercmd has `serverTime <= commandTime` and is
rejected by the server.  The client appears frozen with `ping=999` for up to
several seconds until slow drift corrects.

**Fix:** `cl_adaptiveTiming` scales FAST threshold with `snapshotMsec` (32 ms
at 60 Hz vs 100 ms hardcoded), reducing over-correction frequency.  The
commandTime floor in `CL_RunFrame` (custom servers only) snaps `cl.serverTime`
and `cl.serverTimeDelta` up immediately when the floor is violated, instead of
waiting for slow drift.

### FM-6: Timing loop never settles at high sv_fps (oscillation)

**Cause:** Slow-drift step of 1 ms/snapshot is 6% per snapshot at 16 ms
interval.  Aggressive over-correction cycles between +1 and -1 ms/snap,
never converging.

**Fix:** `cl_adaptiveTiming` halves the backward drift step to 0.5 ms/snap
when `snapshotMsec < 30`, giving the loop time to observe a consistent trend
before committing.

### FM-7: Spurious antiwarp injection on map_restart

**Cause:** `awLastThinkTime` held the pre-restart value.  After 4 warmup
frames (total ~400 ms of `sv.gameTime` advance), `awGap = 400 ms >> awTol
(16 ms)`, so antiwarp fired for every client on the very first active tick.
In mode 2 this immediately zeroed movement inputs.

**Initial (flawed) fix:** Reset `awLastThinkTime = sv.gameTime` in the
`SV_MapRestart_f` final loop.  This worked only if the client responded in
under one game tick (16 ms).  On WAN, the round-trip to first usercmd is
50-200 ms, so antiwarp still fired.

**Final fix:** Reset `awLastThinkTime = 0` to activate the
"never received a real command yet" guard:

```c
if ( awCl->awLastThinkTime == 0 ) continue;
```

Antiwarp engages only after `SV_ClientThink` records the first real usercmd,
at which point `awLastThinkTime = sv.gameTime` and the next tick's `awGap =
gameMsec = awTol` -- exactly at threshold, no injection.

---

## Vanilla Server Behavior

On vanilla URT servers (`sv_snapshotFps` absent from serverinfo):

- `cl_adaptiveTiming` is disabled (`cl.serverForbidsAdaptiveTiming` via
  `sv_allowClientAdaptiveTiming`).
- QVM patch bitmask: `cl_qvmPatchVanilla` (default `3` = bits 0+1 = Patches 2+3).
- Patch 1 (bit 2) is NOT applied: vanilla servers do not anchor `trTime`, so
  the TR_LINEAR formula would compute `dt = cg.time - 0 = enormous` and
  teleport all entities.
- `cg_smoothClients = 1` is NOT forced: vanilla servers do not set TR_LINEAR in
  snapshots, so `cg_smoothClients=1` would cause all player entities to use a
  stale/zero `trDelta`, freezing them in place.

This is enforced at two levels:
1. `cl_qvmPatchVanilla` default excludes bit 2.
2. The `if (!isVanilla)` guard in `VM_URT43_CgamePatches` prevents the
   `cg_smoothClients` force even if someone sets `cl_qvmPatchVanilla 7`.

---

## Cvar Reference

| Cvar | Side | Default | Description |
|---|---|---|---|
| `sv_smoothClients` | Server | `1` | Convert TR_INTERPOLATE player snapshots to TR_LINEAR with velocity |
| `sv_velSmooth` | Server | `0` | EMA velocity smoothing window (ms). 0=disabled |
| `sv_bufferMs` | Server | `0` | Position delay (ms). -1=auto (one snap interval). 0=disabled |
| `cl_adaptiveTiming` | Client | `1` | Scale timing thresholds with snapshotMsec. 0=vanilla |
| `sv_allowClientAdaptiveTiming` | Server | `1` | Allow client to use cl_adaptiveTiming (sent in serverinfo) |
| `cl_urt43cgPatches` | Client | `7` | QVM patch bitmask for custom servers (bits 0+1+2 = all three patches) |
| `cl_qvmPatchVanilla` | Client | `3` | QVM patch bitmask for vanilla servers (bits 0+1 = Patches 2+3 only) |
| `cl_urt43serverIsVanilla` | Client | *(CVAR_TEMP)* | Bridge set by CL_ParseServerInfo; chooses which bitmask cvar to use |
| `sv_antiwarp` | Server | `0` | Engine antiwarp mode. 1=constant, 2=decay |
| `sv_antiwarpTol` | Server | `0` | Antiwarp tolerance (ms). 0=auto (one game frame) |
