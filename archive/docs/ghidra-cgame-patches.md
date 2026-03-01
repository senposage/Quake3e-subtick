# Ghidra cgame Patch Handoff — UT 4.3 QVM Binary Patching

**Date:** 2026-02-27  
**Context:** Quake3e-subtick engine running at `sv_fps 60 / sv_snapshotFps 60`  
**Target binary:** UT 4.3 cgame QVM (pk3)  
**Source reference:** UT 4.2 open source (line numbers are 4.2 — offsets will differ in 4.3 binary)

---

## Overview

The engine now sends snapshots at 60Hz (16ms intervals) instead of vanilla 20Hz (50ms). The server-side is handled — positions are extrapolated between game frames, ring buffer smoothing is available, and client time sync has been fixed. But the **cgame QVM** still has three hardcoded assumptions from the 20Hz era that cause visual artifacts on other players' movement:

1. **frameInterpolation is unclamped** — lerp overshoots past 1.0 at tight windows → position pops forward then snaps back
2. **TR_INTERPOLATE ignores velocity** — when a snapshot is late, players freeze in place instead of extrapolating
3. **No fallback when nextSnap is NULL** — the "extrapolation" code path just freezes

These three patches work together. **Patch 2 can be applied alone** for an immediate win. Patches 1 + 3 must be applied together.

---

## Patch Dependency Order

```
Apply in this order:
  Patch 2 (frameInterpolation clamp) ← standalone, apply first
  Patch 1 (TR_INTERPOLATE velocity extrapolation) ← prerequisite for Patch 3
  Patch 3 (nextSnap == NULL fallback) ← requires Patch 1
```

---

## Patch 2: Clamp frameInterpolation [0.0, 1.0]

**Priority:** HIGHEST — standalone, immediate visual improvement  
**Risk:** Extremely low — worst case: entity holds at final interpolation position for 1 frame instead of overshooting  
**File:** `cg_ents.c` — function `CG_AddPacketEntities` (4.2 source line ~1332-1340)

### The Problem

```c
// Current code in CG_AddPacketEntities:
if ( cg.nextSnap ) {
    delta = cg.nextSnap->serverTime - cg.snap->serverTime;
    cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
    // NO CLAMP — value can be > 1.0 or < 0.0
}
```

At 20Hz (50ms windows), `cg.time` rarely drifts more than 1-2ms past the boundary — `frameInterpolation` might hit 1.04 max, which is barely visible. At 60Hz (16ms windows), the same 1-2ms drift gives `frameInterpolation = 1.12` — a 12% overshoot. Every entity being interpolated jumps 12% past its target position, then snaps back when the next snapshot is processed.

This is the single biggest source of visible "popping" on other players at high snapshot rates.

### The Fix

```c
// Patched code:
if ( cg.nextSnap ) {
    delta = cg.nextSnap->serverTime - cg.snap->serverTime;
    cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
    // CLAMP to valid range
    if ( cg.frameInterpolation < 0.0f ) {
        cg.frameInterpolation = 0.0f;
    }
    if ( cg.frameInterpolation > 1.0f ) {
        cg.frameInterpolation = 1.0f;
    }
}
```

### Ghidra Notes

- `cg.frameInterpolation` is a `float` field in the `cg_t` struct. In 4.2 source it's declared in `cg_local.h`. Find the struct offset by searching for the division result store.
- The computation is a float divide: `(float)(int_sub) / (float)(int_sub)`. In x86 this will be `cvtsi2ss` → `divss` → store. The clamp goes right after the store.
- x86 asm for the clamp (after the `divss` + store):
  ```asm
  ; result is in xmm0 after divss
  xorps   xmm1, xmm1           ; xmm1 = 0.0
  maxss   xmm0, xmm1           ; clamp lower to 0.0
  movss   xmm1, [one_float]    ; xmm1 = 1.0 (find or create a 1.0f constant)
  minss   xmm0, xmm1           ; clamp upper to 1.0
  movss   [cg+offset], xmm0    ; store clamped result
  ```
- If there's not enough space inline, you can jump to a code cave, do the clamp, jump back.
- The `1.0f` constant in IEEE 754: `0x3F800000`. Search the binary for existing float constants or place one in the code cave.

### How to Find in Binary

1. Search for the pattern: integer subtraction → `cvtsi2ss` → integer subtraction → `cvtsi2ss` → `divss`. This is the `(cg.time - snap->serverTime) / (nextSnap->serverTime - snap->serverTime)` computation.
2. It's inside a function that iterates over snapshot entities (large function, lots of entity processing). Should be near the top of `CG_AddPacketEntities`.
3. The conditional check `if ( cg.nextSnap )` guards it — look for a null pointer check on a global struct field before the division.

---

## Patch 1: TR_INTERPOLATE Velocity Extrapolation

**Priority:** HIGH — required for Patch 3  
**Risk:** Low — only changes behavior when `atTime > trTime` (which only happens during extrapolation, i.e. when nextSnap is NULL). Normal interpolation still uses `CG_InterpolateEntityPosition` which doesn't call this path.  
**File:** `bg_misc.c` — function `BG_EvaluateTrajectory` (4.2 source line ~2000-2050)

### The Problem

```c
// Current code in BG_EvaluateTrajectory:
case TR_INTERPOLATE:
    VectorCopy( tr->trBase, result );   // just copies position — ignores velocity entirely
    break;
```

`trDelta` contains the entity's velocity (set by `BG_PlayerStateToEntityState` at bg_misc.c:2041: `s->pos.trDelta = ps->velocity`). It's always there. It's just never read for `TR_INTERPOLATE`. When the cgame has no `nextSnap` and falls through to `BG_EvaluateTrajectory`, the entity freezes at `trBase` — the position from the last snapshot. The player appears to stop dead for one frame, then jumps to catch up.

### The Fix

```c
// Patched code:
case TR_INTERPOLATE:
    deltaTime = ( atTime - tr->trTime ) * 0.001f;   // ms to seconds
    if ( deltaTime > 0.0f && deltaTime < 0.1f ) {   // max 100ms extrapolation safety cap
        // Forward-extrapolate using velocity
        result[0] = tr->trBase[0] + tr->trDelta[0] * deltaTime;
        result[1] = tr->trBase[1] + tr->trDelta[1] * deltaTime;
        result[2] = tr->trBase[2] + tr->trDelta[2] * deltaTime;
    } else {
        VectorCopy( tr->trBase, result );   // fallback: too old or negative time
    }
    break;
```

This is equivalent to `VectorMA(trBase, deltaTime, trDelta, result)` but written out for clarity.

### Ghidra Notes

- `BG_EvaluateTrajectory` is a switch statement on `tr->trType`. Find the `TR_INTERPOLATE` case — it's the simplest one: just a `VectorCopy` (3x float copy, no math).
- `trajectory_t` struct layout (from bg_public.h):
  ```
  typedef struct {
      trType_t  trType;       // offset +0,  4 bytes (int enum)
      int       trTime;       // offset +4,  4 bytes
      int       trDuration;   // offset +8,  4 bytes
      vec3_t    trBase;       // offset +12, 12 bytes (3 floats)
      vec3_t    trDelta;      // offset +24, 12 bytes (3 floats)
  } trajectory_t;             // total: 36 bytes
  ```
- `TR_INTERPOLATE` enum value: in UT 4.2 this is `1` (TR_STATIONARY=0, TR_INTERPOLATE=1, TR_LINEAR=2, etc.). Verify in the 4.3 binary — the switch jump table will tell you.
- The patched code needs: `atTime` (function parameter, int), `tr->trTime` (offset +4, int), `tr->trBase` (offset +12, 3 floats), `tr->trDelta` (offset +24, 3 floats).
- The `0.001f` constant in IEEE 754: `0x3A83126F`. The `0.1f` constant: `0x3DCCCCCD`.
- There should be enough room in the switch case since the original is just 3 float copies. If not, jump to a code cave.

### How to Find in Binary

1. `BG_EvaluateTrajectory` takes 3 params: `(trajectory_t *tr, int atTime, vec3_t result)`. It's called from many places — find it by searching for the `TR_LINEAR` case which does `trBase + trDelta * (atTime - trTime) * 0.001` — that's a distinctive float multiply pattern.
2. The `TR_INTERPOLATE` case is typically right above or below `TR_LINEAR` in the switch. It's the one that does 3 consecutive float copies with no arithmetic.
3. Cross-reference: `BG_EvaluateTrajectoryDelta` is usually the function immediately after `BG_EvaluateTrajectory` in the binary — it has the same switch structure but returns velocity instead of position. Its `TR_INTERPOLATE` case does `VectorClear(result)` (3x store 0.0).

### Safety Cap Explanation

The `0.1f` (100ms) cap prevents runaway extrapolation if a snapshot is extremely late. At sv_fps 60, a normal gap is 16ms. If we haven't received a snapshot in 100ms (6+ snapshots missed), something is seriously wrong and extrapolating further would just make the player fly off in a straight line. Better to freeze at that point.

---

## Patch 3: nextSnap == NULL Extrapolation Fallback

**Priority:** HIGH — but only useful with Patch 1 applied  
**Risk:** Low — only changes what happens when `nextSnap == NULL`, which currently produces a visible freeze. The new behavior (extrapolation) is always better than freezing.  
**File:** `cg_ents.c` — function `CG_CalcEntityLerpPositions` (4.2 source line ~1080-1119)

### The Problem

```c
// Current code in CG_CalcEntityLerpPositions:
if ( cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE ) {
    CG_InterpolateEntityPosition( cent );   // REQUIRES nextSnap != NULL — crashes if null
    return;
}
// fallthrough to BG_EvaluateTrajectory — which for TR_INTERPOLATE just does VectorCopy (freeze)
```

AND in `CG_InterpolateEntityPosition` (4.2 source line ~1050):
```c
static void CG_InterpolateEntityPosition( centity_t *cent ) {
    // ...
    if ( !cg.nextSnap ) {
        CG_Error( "CG_InterpolateEntityPosition: !cg.nextSnap" );  // CRASH
    }
    // ...
}
```

So when `nextSnap == NULL`:
- If `cent->interpolate` is true AND trType is `TR_INTERPOLATE` → calls `CG_InterpolateEntityPosition` → **crash** (CG_Error)
- If `cent->interpolate` is false → falls through to `BG_EvaluateTrajectory` → `TR_INTERPOLATE` case → `VectorCopy(trBase)` → **freeze**

At 20Hz (50ms windows), `nextSnap == NULL` is rare. At 60Hz (16ms windows), any packet arriving 1-2ms late means `nextSnap` hasn't been parsed yet when the render frame runs. This happens frequently.

### The Fix

```c
// Patched code in CG_CalcEntityLerpPositions:
if ( cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE ) {
    if ( cg.nextSnap ) {
        CG_InterpolateEntityPosition( cent );   // normal path — lerp between snapshots
    } else {
        // Extrapolation fallback — use velocity to project forward from last known position
        // This calls BG_EvaluateTrajectory which (with Patch 1) now does:
        //   trBase + trDelta * (cg.time - trTime) * 0.001
        BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
        BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );
    }
    return;
}
```

### Ghidra Notes

- `CG_CalcEntityLerpPositions` is called per-entity from `CG_AddPacketEntities`. It takes a single `centity_t *cent` parameter.
- The key pattern to find: check `cent->interpolate` (bool/int field), then check `cent->currentState.pos.trType == TR_INTERPOLATE (1)`, then call `CG_InterpolateEntityPosition`.
- The patch adds a null check on `cg.nextSnap` (global pointer in the `cg_t` struct) before the call:
  - If not null → call `CG_InterpolateEntityPosition` as before
  - If null → call `BG_EvaluateTrajectory` twice (once for position, once for angles) and return
- `centity_t` key offsets (approximate — verify in binary):
  - `cent->interpolate`: bool/int field
  - `cent->currentState`: embedded `entityState_t` — `pos` is a `trajectory_t` at the start
  - `cent->lerpOrigin`: `vec3_t` (3 floats) — write target for position
  - `cent->lerpAngles`: `vec3_t` (3 floats) — write target for angles
- `BG_EvaluateTrajectory` address: you already found this for Patch 1. Same function, two calls with different trajectory pointers (`pos` and `apos`).

### How to Find in Binary

1. Find `CG_InterpolateEntityPosition` first — it's the one that calls `CG_Error` with the string "CG_InterpolateEntityPosition: !cg.nextSnap". Search for that string in the binary's rodata section.
2. Find the **caller** of `CG_InterpolateEntityPosition` — that's `CG_CalcEntityLerpPositions`. There should be only one or two call sites.
3. The patch goes right at that call site: add the `cg.nextSnap` null check before the call, with the `BG_EvaluateTrajectory` fallback in the else branch.

### Also Recommended: Remove the CG_Error Crash

While you're in `CG_InterpolateEntityPosition`, the `CG_Error` call when `nextSnap == NULL` should be changed to a graceful return:

```c
// Before:
if ( !cg.nextSnap ) {
    CG_Error( "CG_InterpolateEntityPosition: !cg.nextSnap" );
}

// After:
if ( !cg.nextSnap ) {
    BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
    BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );
    return;
}
```

This is a safety net — with Patch 3 applied, this path should never be reached (the null check happens before the call). But defense in depth: if something else calls `CG_InterpolateEntityPosition` directly, it won't crash.

---

## Verification / Test Matrix

After applying patches, test these scenarios:

| Scenario | What to Watch | Expected Before Patch | Expected After Patch |
|---|---|---|---|
| Normal play, sv_fps 60 | Other players' movement smoothness | Periodic micro-pops every few frames | Smooth continuous movement |
| Packet loss (net_droppackets 1-2) | Other players during dropped packets | 1-frame freeze then position jump | Smooth extrapolation through the gap |
| Fast direction change | Player strafing back and forth | Overshoot past turn point, snap back | Clean stops at turn point |
| Spectator follow mode | Followed player's movement | Same artifacts as normal play | Smooth (CG_InterpolatePlayerState also benefits) |
| Demo playback | All entity movement | Frame pops visible in slow-mo | Clean interpolation |
| Long packet loss (>100ms) | Other players | Extended freeze | Freeze after 100ms cap (intentional safety) |

### Debug Commands

```
cl_showTimeDelta 1          — verify client time sync is stable (should see steady values, no <RESET> or <FAST>)
cg_drawSnapshot 1           — shows snapshot timing info  
net_droppackets X           — simulate packet loss (X = drop every Nth packet)
```

---

## Architecture Reference

For context on how these patches fit into the full pipeline:

```
Server Side (engine — already done):
  sv_gameHz 20 (QVM runs at 20Hz)
  ↓
  sv_fps 60 (engine ticks at 60Hz)
    → sv_extrapolate: forward-extrapolate player trBase between game frames
    → sv_bufferMs: optional position delay from ring buffer
    → sv_smoothClients: optional TR_LINEAR trajectory with smoothed velocity
  ↓
  sv_snapshotFps 60 (snapshots sent at 60Hz)
  ↓
  Network (16ms between packets)
  ↓
Client Side (engine — fixed by removing cl_snapScaling):
  Vanilla Q3 time sync: +1ms/-2ms drift, 500ms reset, 100ms fast adjust
  cl.serverTime stays ~3ms behind snapshot boundary
  ↓
cgame QVM (THESE PATCHES):
  Patch 2: frameInterpolation clamped [0.0, 1.0] — no overshoot
  Patch 1: TR_INTERPOLATE reads velocity — extrapolation possible
  Patch 3: nextSnap == NULL → extrapolate instead of freeze
  ↓
  Rendered player positions: smooth
