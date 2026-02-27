# Debug Session: sv_smoothClients / sv_bufferMs Stationary Jitter
**Date:** 2026-02-27
**Participants:** senposage, GitHub Copilot

---

## Problem Statement

After merging the cl_snapScaling removal and sv_smoothClients/sv_bufferMs composability changes, testing revealed two sources of visible vibration/jitter on **stationary players**:

1. `sv_smoothClients 1` causes stationary player vibration
2. `sv_bufferMs -1` causes the same vibration effect

Both features default to off (0), so production is unaffected. The jitter was investigated to determine root cause and whether the features are worth fixing.

## Root Cause: Pmove Ground-Snapping Micro-Oscillations

Stationary players are not truly stationary at the engine level. Pmove's ground snapping produces tiny velocity oscillations every tick — the player's position micro-adjusts as gravity, ground trace, and snap-to-ground interact. These velocities are small (< 10 ups) but non-zero.

### sv_smoothClients Path

In `SV_BuildCommonSnapshot` (sv_snapshot.c), when `sv_smoothClients 1`:

```c
if ( DotProduct( finalVel, finalVel ) > 1.0f ) {
    VectorCopy( origin, es->pos.trBase );
    VectorCopy( finalVel, es->pos.trDelta );
    es->pos.trType = TR_LINEAR;
    es->pos.trTime = sv.time;
}
```

The dead-zone threshold of `1.0f` (velocity > 1 ups) is too sensitive. Pmove ground-snap micro-velocities oscillate across this threshold on some frames but not others. This causes the entity to flip between `TR_LINEAR` and `TR_INTERPOLATE` every few ticks — the cgame handles each trajectory type completely differently, producing visible jitter.

### sv_bufferMs Path

The ring buffer path has **no dead-zone check at all**. When `usedBuffer` is true:

```c
if ( usedBuffer ) {
    VectorCopy( origin, es->pos.trBase );
    VectorCopy( velocity, es->pos.trDelta );
}
```

It blindly copies whatever position/velocity the ring buffer interpolated — including the micro-oscillating positions from different buffer slots.

## Fix

1. **Raise dead-zone threshold** from `1.0f` to `100.0f` (DotProduct of velocity with itself, i.e. velocity magnitude² > 100.0f = velocity > 10 ups). Filters out all idle ground-snap jitter. Walking speed in UT is ~270 ups, so 10 ups is well below any intentional movement.

2. **Add dead-zone guard to sv_bufferMs path**. When velocity from the buffer is below threshold, keep the original QVM position instead of copying from the buffer.

**PR:** Fix stationary player jitter from sv_smoothClients and sv_bufferMs

## Decision: Default Both to Off

Both `sv_smoothClients` and `sv_bufferMs` default to 0. With `sv_extrapolate 1` providing fresh positions every tick via direct `ps->origin` reads, there's less need for either feature:

- `sv_extrapolate 1` already gives unique positions per snapshot (no stale data)
- `sv_smoothClients` (TR_LINEAR) was meant to let cgame extrapolate between snapshots, but sv_extrapolate makes snapshots already have unique positions
- `sv_bufferMs` adds latency for smoothing that isn't needed when positions are already smooth

The dead-zone fix makes them usable if needed, but the recommended config is `sv_smoothClients 0`, `sv_bufferMs 0`, `sv_extrapolate 1`.

## Additional: cg_smoothClients QVM Intercept

The UT 4.3 QVM's `cg_smoothClients` defaults to 0, which forces all player entities to `TR_INTERPOLATE` in the cgame — overriding any TR_LINEAR the server injects via `sv_smoothClients 1`.

The engine can't change the QVM's default directly, but it CAN block the QVM from setting it. The same pattern used to block `snaps` in `cl_cgame.c`:

```c
case CG_CVAR_SET:
    // exclude some game module cvars
    if (!Q_stricmp((char *)VMA(1), "snaps") ||
        !Q_stricmp((char *)VMA(1), "cg_smoothClients")) {
        return 0;
    }
```

This intercepts the QVM's `trap_Cvar_Set("cg_smoothClients", ...)` syscall and blocks it, letting the engine control the default. Not needed now (sv_smoothClients 0 sends TR_INTERPOLATE anyway), but gives us the option later.

A similar pattern is used server-side in `sv_game.c` to block the QVM from setting `sv_fps` and `g_failedvotetime`.

**PR:** Block QVM from overriding cg_smoothClients default

## Explored: VM Intercept for frameInterpolation Clamp

Investigated whether the `trap_Cvar_Set` intercept pattern could fix the cgame's unclamped `frameInterpolation` (Patch 2 from ghidra-cgame-patches.md). 

**Answer: No.** The `frameInterpolation` computation is entirely internal to the QVM:

```c
f = (float)(cg.time - cg.snap->serverTime) / (cg.nextSnap->serverTime - cg.snap->serverTime);
```

This never makes a syscall — no `trap_Cvar_Set`, no `trap_Cvar_Get`, no `trap_` anything. The engine never sees it. The VM intercept pattern only works for syscalls.

**Possible alternatives:**
1. **Ghidra QVM binary patch** — find the float division and inject a clamp (recommended, documented in ghidra-cgame-patches.md)
2. **Engine-side override** — compute frameInterpolation in the engine and force it into QVM memory (very invasive, rejected)
3. **Hide the problem** — ensure snapshots arrive fast enough that cg.time never exceeds nextSnap->serverTime (what sv_extrapolate + vanilla time sync mostly does)

Currently relying on option 3. The Ghidra patch remains the proper fix.

## Key Lessons

1. **Stationary players aren't truly stationary.** Pmove ground snapping creates micro-velocities that trip any sensitive threshold. Dead-zones must account for this.

2. **VM intercept pattern only works for syscalls.** Anything the QVM computes internally (frameInterpolation, interpolation logic) is invisible to the engine. Only `trap_*` calls can be intercepted.

3. **sv_extrapolate 1 is the primary smoothing mechanism.** It reads `ps->origin` directly (updated every Pmove tick), giving each snapshot a unique position. sv_smoothClients and sv_bufferMs are secondary/experimental.
