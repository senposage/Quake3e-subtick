# frameInterpolation Clamp — Engine-Side Fix

**Date:** 2026-03-01  
**Status:** FIXED (engine-side)  
**Related:** `archive/docs/ghidra-cgame-patches.md` — Patch 2

---

## Problem

The UT4.3 cgame QVM computes `cg.frameInterpolation` in `CG_AddPacketEntities` without clamping:

```c
// QVM code (cg_ents.c — cannot be modified without Ghidra):
if ( cg.nextSnap ) {
    delta = cg.nextSnap->serverTime - cg.snap->serverTime;
    cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
    // NO CLAMP — value can be > 1.0 or < 0.0
}
```

When `cg.time` drifts even 1–2 ms past `cg.nextSnap->serverTime`, the ratio overshoots 1.0.
The magnitude of this overshoot scales inversely with the snapshot window:

| Snapshot Rate | Window | 2 ms drift → frameInterpolation |
|---|---|---|
| 20 Hz | 50 ms | **1.04** (barely visible) |
| 60 Hz | 16 ms | **1.125** (12.5% overshoot — clearly visible) |

Every entity being interpolated jumps 12% past its target position, then snaps back
when the next snapshot is processed.  This is the single biggest source of visible
"popping" on other players at high snapshot rates.

---

## Root Cause

The engine's `CL_SetCGameTime()` previously capped `cl.serverTime` at
`cl.snap.serverTime + cl.snapshotMsec` (one full interval ahead of the latest received
snapshot).  This allowed the QVM to see `cg.time > cg.nextSnap->serverTime`, keeping
`cg.nextSnap` non-NULL while producing `frameInterpolation > 1.0`.

---

## Engine-Side Fix

**File:** `code/client/cl_cgame.c` — `CL_SetCGameTime()`  
**Commit:** This change

The serverTime cap was tightened from `cl.snap.serverTime + cl.snapshotMsec` to
`cl.snap.serverTime - 1` (via an intermediate step at exactly `cl.snap.serverTime`):

```c
// Before (allowed frameInterpolation > 1 at 60 Hz):
if ( cl.serverTime - (cl.snap.serverTime + cl.snapshotMsec) > 0 ) {
    cl.serverTime = cl.snap.serverTime + cl.snapshotMsec;
}

// Intermediate (fixed overshoot; introduced intermittent EXTRAP — see below):
if ( cl.serverTime > cl.snap.serverTime ) {
    cl.serverTime = cl.snap.serverTime;
}

// Final (prevents both overshoot and the QVM snapshot-boundary EXTRAP):
if ( cl.serverTime >= cl.snap.serverTime ) {
    cl.serverTime = cl.snap.serverTime - 1;
}
```

The intermediate fix at exactly `cl.snap.serverTime` introduced a new intermittent
symptom: whenever the cap fired, `cg.time` equalled `cg.nextSnap->serverTime` exactly.
The QVM's `CG_TransitionSnapshot` advances the snapshot window the moment
`cg.time >= cg.nextSnap->serverTime`.  After advancing, the QVM looked for the next
snapshot (one beyond `cl.snap`), found none buffered yet, set `cg.nextSnap = NULL`,
and entered EXTRAP mode — producing the same INTERP/EXTRAP flicker as before, but
only in transient bursts (network jitter, frame spike, or `sv_fps` change) rather than
continuously.

Capping one millisecond short keeps `cg.time` strictly below `cg.nextSnap->serverTime`
at all times; the QVM never transitions prematurely.

This is the engine-side equivalent of the QVM binary Patch 2 described in
`archive/docs/ghidra-cgame-patches.md`.

`cl.frameInterpolation` (the engine-side estimate used by the net monitor widget) is
also explicitly clamped to `[0, 1]` as a defence-in-depth measure.

---

## Behaviour Change

| Scenario | Before | After |
|---|---|---|
| Normal play, sv_fps 60 | Entity positions overshoot target by up to 12%, then snap back | Smooth: entities hold at ~94% interpolation until next snapshot |
| Network jitter / sv_fps change | serverTimeDelta spike → cap fires → QVM enters EXTRAP briefly | cap fires at snap-1 → cg.time stays in INTERP window, no EXTRAP |
| Alt-tab / large realtime spike | serverTime jumped forward by many seconds, large overshoot | Clamped immediately |

The tradeoff: when the cap is actively holding, entities hold at ~94% interpolation
(60 Hz) rather than exactly 100%.  This is imperceptible in practice.

---

## Relationship to Ghidra QVM Patches

`archive/docs/ghidra-cgame-patches.md` describes three binary patches for the cgame QVM:

- **Patch 2 (frameInterpolation clamp):** The engine-side fix here makes Patch 2 optional
  for `frameInterpolation` correctness.  If Patch 2 is later applied to the QVM binary,
  the engine cap can optionally be loosened back to `+snapshotMsec` to re-enable
  extrapolation for Patches 1 and 3.

- **Patches 1+3 (TR_INTERPOLATE extrapolation / nextSnap NULL fallback):** These patches
  require `cg.time > cg.nextSnap->serverTime` to trigger the extrapolation path.  With the
  engine cap at `cl.snap.serverTime`, those patches have no effect until the cap is loosened.
  This is intentional: Patches 1+3 should only be enabled together with Patch 2.

### Recommended upgrade path when Ghidra patches are available

1. Apply all three Ghidra QVM patches (Patch 2 first, then Patches 1+3).
2. Loosen the engine cap back to `cl.snap.serverTime + cl.snapshotMsec`.
3. The QVM's own clamp (Patch 2) prevents the overshoot; the extrapolation path
   (Patches 1+3) handles late snapshots gracefully.
