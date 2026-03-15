# UrT 4.3 QVM Shadow Bug — Analysis & Implementation Notes

**Status:** Implemented — Patch 1 (bit 2) upgraded with `trTime == 0` guard (widest coverage).

---

## Summary

The shadow displacement bug ("shadow moves away when moving, returns when stopped") is
present even in offline `/map` play, confirming it is a **cgame (client-side) issue**,
not a server-side issue. PR #119's server-side origin-anchor fix did not resolve it
because the root cause is in the QVM rendering path.

---

## Key QVM Structure Facts (UrT 4.3 cgame.qvm CRC 0x1289DB6B)

### `centity_t` layout (derived from disassembly)

| Offset | Field | Notes |
|--------|-------|-------|
| `0x000` | `previousEvent` | 4 bytes |
| `0x004` | `miscTime` | 4 bytes |
| `0x008` | `currentState` | 208 bytes (`entityState_t`) |
| `0x0d8` | `nextState` | 208 bytes (`entityState_t`) |
| `0x1a8` | `pe` (playerEntity_t) + other fields | ~2292 bytes |
| `0xa9c` | `lerpOrigin[0]` (x) | float |
| `0xaa0` | `lerpOrigin[1]` (y) | float |
| `0xaa4` | `lerpOrigin[2]` (z) | float |
| `0xaa8` | *(end)* | `sizeof(centity_t) = 0xaa8 = 2728` |

Derived sub-offsets within `currentState` (`entityState_t` at base+0x008):

| centity_t offset | entityState_t field |
|------------------|---------------------|
| `0x008` | `number` |
| `0x00c` | `eType` |
| `0x010` | `eFlags` |
| `0x014` | `pos.trType` (trajectory) |
| `0x038` | `apos.trType` |
| `0x064` | `origin[0]` (x) |
| `0x068` | `origin[1]` (y) |
| `0x06c` | `origin[2]` (z) |
| `0x0b0` | `clientNum` |

---

## Shadow Function — Instruction 0x1dd00

The shadow rendering function begins at QVM instruction **0x1dd00** (frame size `0xe0`).

**Called from:** `0x1df4b` (×4) and `0x1e7a7` (×4)  
Both callers are dispatched by the entity-type renderer (`0x15cac`) for entity types 1 and 5.

### What the shadow function does

```
1. Read  cg_shadows.integer → if 0, return early (no shadow)
2. Copy  cent->lerpOrigin  (arg0 + 0xa9c)  →  end_pt   ; end.z -= 24
3. Check trap_CM_PointContents(end_pt) for water/lava (to handle looping sounds)
4. Copy  cent->lerpOrigin  (arg0 + 0xa9c)  →  start_pt ; start.z += 32
5. trap_CM_BoxTrace(trace, start_pt, end_pt, NULL, NULL, 0, 0x38)
6. If trace.fraction == 1.0 → return (no floor hit, no shadow)
7. Build 4-vertex quad centred on lerpOrigin±32 units in X/Y at trace.endpos.z
8. trap_R_AddPolyToScene(shader, 4, quad)
```

**The shadow already reads `lerpOrigin` (offset `0xa9c`)**, NOT `currentState.origin`
(offset `0x64`).  With Patch 1 (TR_INTERPOLATE→TR_LINEAR) active, `lerpOrigin` is
velocity-extrapolated and should match the body position.

---

## Why the Shadow Still Lags (Root Cause Hypothesis)

Despite reading `lerpOrigin`, the shadow can still appear displaced because the **body
rendering in CG_Player (instr 0x17172) applies additional client-side offsets** (bob,
lean, view-height adjustment) that are NOT reflected in `lerpOrigin`.  The shadow quad
is anchored to plain `lerpOrigin`, while the visible model origin may be slightly
different.

A second, more likely mechanism for offline (`/map`) play:

In offline play the local player entity is built from `cg.predictedPlayerState.origin`
and placed directly into `cg.predictedPlayerEntity`.  The entity dispatcher
(`0x15cac`) calls `CG_CalcEntityLerpPositions` (`0x1594f`) which evaluates
`BG_EvaluateTrajectory` for the entity's trajectory.  For the **predicted player
entity**, `pos.trBase` = `ps.origin` and `pos.trDelta` = `ps.velocity`.  With Patch 1
active, the extrapolation term `vel × (cg.time − trTime)` is applied.  If `trTime` is
NOT being updated each frame to match `ps.commandTime`, this term accumulates and the
shadow overshoots the body.

---

## All QVM Uses of `currentState.origin` (offset 0x64)

A search of the disassembly for `(arg0|base) + 0x64` with a 12-byte `block_copy`
(VectorCopy pattern) identified the following sites:

| Instruction | Function | Purpose |
|-------------|----------|---------|
| `0x16120–0x16124` | `0x160f9` (entity-type-18 renderer) | Copies `cent->currentState.origin` into a player trail / position buffer used for entity type 18 (UrT-specific). |
| `0x1212e–0x1213b` (approx) | `0x11edb` (CG_ImpactMark-like) | Copies origin for a forward-direction trace (weapon impact, not shadow). |

**Neither of these is the shadow function.**  The shadow function exclusively uses
`lerpOrigin` (`0xa9c`).

---

## Entity Type Dispatch Table (0x15cac)

| Type | Jump target | Description |
|------|-------------|-------------|
| 0 | `0x15d89` | ET_GENERAL — calls `0x14168` |
| 1 | `0x15d91` | ET_PLAYER — calls `0x1f155`, then `0x1e7a7` (renders body + shadow) |
| 5 | `0x15e2e` | ET_MISSILE? — calls `0x1df4b` (also renders shadow) |
| 12 | `0x15e66` | UrT entity — calls `0x159fc` |
| 16 | `0x15e6e` | UrT entity — calls `0x160f9` (type-18 sub-handler) |
| 17 | `0x15ce6` | UrT entity — complex path |
| 18 | `0x15e76` | UrT entity — calls `0x160f9` |

Before any type-specific call, `0x15cac` always calls:
1. `0x1594f` — **CG_CalcEntityLerpPositions** (sets `lerpOrigin` via BG_EvaluateTrajectory)
2. `0x1404e` — secondary setup

---

## Existing Patches and Their Relationship to the Shadow

### Patch 1 (bit 2) — BG_EvaluateTrajectory TR_INTERPOLATE → TR_LINEAR
Redirects switch-table case 1 (`TR_INTERPOLATE`) to the TR_LINEAR extrapolation
handler.  **This is the patch that causes the shadow displacement**: with
`TR_INTERPOLATE` mapped to `TR_LINEAR`, `lerpOrigin` is pushed forward by
`vel × dt`.  The shadow reads this `lerpOrigin` and so moves with the extrapolated
position — but the *visual* body model may be anchored differently during transitions.

### Patch 2 (bit 0) — frameInterpolation clamp
Fixes interpolation fraction (0.0–1.0 clamp). Unrelated to shadow displacement.

### Patch 3 (bit 1) — CG_InterpolateEntityPosition null-nextSnap crash
Safety fix; unrelated to shadow displacement.

---

## Implemented Fix (Patch 1 upgrade)

### Strategy: widest-coverage instruction guard in `BG_EvaluateTrajectory`

`BG_EvaluateTrajectory` has a dead `CG_Error` block at instruction index
**0x3b434–0x3b43e** (11 slots) that is only reachable when `trType < 0` or
`trType > 11` — which never happens in normal play.  We repurpose those slots
as a 9-instruction **trTime == 0 guard handler** and redirect the
TR_INTERPOLATE switch-table entry there.

#### Guard code written at 0x3b434

```
LOCAL  0x15c          ; arg0 = trajectory* (frame_size=0x154, so +8 = 0x15c)
LOAD4                 ; dereference → trajectory pointer
CONST  4              ; offsetof(trajectory_t, trTime) = 4
ADD                   ; &trajectory->trTime
LOAD4                 ; trTime value
CONST  0              ; 0
EQ     0x3ab0c        ; if trTime==0 → TR_STATIONARY (VectorCopy, no extrapolation)
CONST  0x3ab15        ; TR_LINEAR case address
JUMP                  ; → velocity extrapolation with valid trTime
IGNORE                ; (was CALL CG_Error)
IGNORE                ; (was POP)
```

#### What this fixes

| Entity | trTime before fix | Result before | Result after |
|--------|------------------|---------------|--------------|
| Remote player (our server) | `sv.time` (set by sv_snapshot.c) | OK (extrapolation valid) | OK (unchanged) |
| Predicted local player | `0` (BG_PlayerStateToEntityState never sets it) | Huge extrapolation → teleport | VectorCopy → correct |
| Any entity with forgotten trTime | `0` | Teleport | VectorCopy → correct |
| Vanilla server (patch disabled) | N/A | Original TR_STATIONARY | Original (patch not applied) |

#### Vanilla-server safety

The entire patch block is gated on `cgPatches & 4`, which reads from
`cl_qvmPatchVanilla` when `cl_urt43serverIsVanilla` is set.  Vanilla servers
that do not anchor `trTime` server-side do not set bit 2, so this code never
runs and the QVM is left unmodified.

---

## Proposed Next Step

A **Patch 4** targeting the shadow function (0x1dd00) is not needed because the
function already reads `lerpOrigin`.  The displacement is likely caused by
`pos.trTime` not being set to `commandTime` / `sv.time` for the predicted player
entity in offline play, causing TR_LINEAR extrapolation to accumulate.

The correct fix is to ensure `pos.trTime` is anchored to `sv.time` (or
`commandTime`) when building the entity trajectory from the playerstate — this is a
**server-side or engine-side** fix, mirroring what Patch 1's documentation already
describes as a pre-condition: *"REQUIRES SERVER-SIDE SUPPORT: sv_snapshot.c must
anchor pos.trTime to sv.time for every TR_INTERPOLATE client entity"*.

For offline play specifically, `CG_PredictPlayerState` in the QVM needs to set
`ps.commandTime` as `trTime` when copying to `currentState.pos`.  This happens in
the game-side `BG_PlayerStateToEntityState` function (at QVM instr **0x1423**).

---

## Files Changed

| File | Change |
|------|--------|
| `docs/project/QVM_SHADOW_ANALYSIS.md` | This document (updated with implemented fix) |
| `code/qcommon/vm.c` | Patch 1 upgraded: adds `URT43_INSTR_TR_GUARD_CASE`, writes 9-instr guard at 0x3b434, redirects TR_INTERPOLATE switch entry to guard instead of directly to TR_LINEAR |
