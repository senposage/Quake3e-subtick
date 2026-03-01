# Debug Session: frameInterpolation Oscillation — Lagometer / Net Monitor Correlation
**Date:** 2026-03-01  
**Participants:** senposage, GitHub Copilot

---

## Problem Statement

At `sv_fps 60 / sv_snapshotFps 60`, the top bar of the in-game `cg_lagometer` was
**rapidly flickering** — alternating between its green/yellow interpolating state and
its yellow/red extrapolating state, frame by frame.  The flicker frequency was high
enough to appear as a continuous colour wash rather than a clean band.

The `cl_netgraph` net monitor widget (toggled with `cl_netgraph 1`) showed the
corresponding `fI` value visibly oscillating between values below 1.0 (INTERP) and
values above 1.0 (EXTRAP) on the same frames where the lagometer top bar changed
colour.

---

## Diagnostic Method: Net Monitor ↔ Lagometer Correlation

The net monitor widget (`cl_netgraph 1`) displays the engine's per-frame estimate of
`frameInterpolation`:

```
fI:   1.043 EXTRAP     ← cl.frameInterpolation > 1.0
fI:   0.912 INTERP     ← cl.frameInterpolation <= 1.0
```

The QVM's `cg_lagometer` independently reads `cg.frameInterpolation` (the actual QVM
value, not the engine estimate) and renders its top bar:

- **Green/Yellow** → `cg.frameInterpolation` ≤ 1.0 (interpolating between snapshots)
- **Yellow/Red**   → `cg.frameInterpolation` > 1.0 (extrapolating past latest snapshot)

Watching both simultaneously confirmed they flickered in perfect sync: whenever the
widget showed `EXTRAP`, the lagometer top bar was also in its warm-colour state.

This established that the engine-side `cl.serverTime` was routinely overshooting
`cl.snap.serverTime`, and the QVM was seeing the same condition via `cg.time`.

---

## Root Cause

The engine's `CL_SetCGameTime()` previously capped `cl.serverTime` at
`cl.snap.serverTime + cl.snapshotMsec` — one full interval ahead of the latest
received snapshot.  `cl.serverTime` is exposed to the QVM as `cg.time`.

At **20 Hz** (50 ms windows): a 1–2 ms overshoot of `cl.snap.serverTime` still keeps
`cg.time` well within the `cg.nextSnap->serverTime` boundary; `cg.frameInterpolation`
reaches at most ~1.04.  Barely visible.

At **60 Hz** (16 ms windows): the same 1–2 ms overshoot pushes `cg.time` past
`cg.nextSnap->serverTime`.  The QVM's unclamped division:

```c
cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime )
                      / (float)( cg.nextSnap->serverTime - cg.snap->serverTime );
```

yields **1.06–1.12** — a 6–12% overshoot.  Every interpolated entity jumps that
percentage past its target, then snaps back when the next snapshot arrives.

The `CL_AdjustTimeDelta()` `+1/-1` slow-drift loop oscillates `cl.serverTime` across
the snapshot boundary every few frames, so the condition fires repeatedly, producing
the rapid INTERP/EXTRAP flicker visible in both the widget and the lagometer.

---

## Fix

**File:** `code/client/cl_cgame.c` — `CL_SetCGameTime()`  
**PR:** Fix frameInterpolation > 1.0 popping at high snapshot rates

The serverTime cap was tightened from `cl.snap.serverTime + cl.snapshotMsec` to
exactly `cl.snap.serverTime`:

```c
// Before — cap was one full interval ahead, still allowed fI > 1.0:
if ( cl.serverTime - (cl.snap.serverTime + cl.snapshotMsec) > 0 )
    cl.serverTime = cl.snap.serverTime + cl.snapshotMsec;

// After — cg.time never exceeds cg.nextSnap->serverTime:
if ( cl.serverTime > cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime;
```

After this fix:
- `cl.frameInterpolation` stays in `[0, 1]` at all snapshot rates.
- The `cg_lagometer` top bar holds steady green (interpolating).
- The `cl_netgraph` widget shows `fI` climbing from ~0 to 1.0 as a clean sawtooth
  then holding at 1.0 until the next snapshot arrives — no EXTRAP flicker.

Full analysis and upgrade path for Ghidra QVM patches: see
`docs/frameinterpolation-clamp-engine-fix.md`.

---

## What the Net Monitor Widget Added

Before this debug session, `cg_lagometer` flicker was visually obvious but not
self-explaining.  The net monitor widget provided:

1. **A numeric fI value** — confirmed the oscillation was specifically crossing 1.0,
   not just noisy within [0,1].
2. **INTERP / EXTRAP label** — made the threshold crossing explicit without needing to
   read QVM internals.
3. **Per-frame timing** — visible on the same frame as the lagometer flicker,
   confirming they were reporting the same underlying value.

The combination of a known-bad visual signal (lagometer) and a numeric engine-side
readout (net monitor) was enough to identify the root cause in a single session.

---

## Residual Intermittent Oscillation (Follow-up, 2026-03-01)

### New Diagnostic Information

After the initial fix, the oscillation was found to be **intermittent** — often absent for
an entire 20-minute round, but reproducible by changing `sv_fps` or related cvars
mid-session.

### Refined Root Cause

The cap at exactly `cl.snap.serverTime` introduced a boundary-touching problem:

- The QVM advances its snapshot window (`cg.snap = cg.nextSnap`) the instant
  `cg.time >= cg.nextSnap->serverTime`.
- `cg.nextSnap->serverTime == cl.snap.serverTime` (engine's current snapshot IS the QVM's
  next interpolation target).
- When the cap fires: `cg.time = cl.snap.serverTime = cg.nextSnap->serverTime` — the
  transition condition is met.
- After the transition, the QVM looks for the snapshot *after* `cl.snap`. That snapshot has
  not arrived yet. The QVM sets `cg.nextSnap = NULL` and enters **EXTRAP mode**.

Under **steady-state play** the cap rarely fires (equilibrium from `+1/-1` drift holds
`cl.serverTime ≈ snap.serverTime - 5ms`), so the oscillation was intermittent rather than
continuous.

**Induction path via `sv_fps` change:** when `sv_fps` drops (e.g. 60→20), no new snapshot
arrives for ~34ms extra. During that wait `cl.serverTime` advances with realtime, hits the
cap at exactly `snap.serverTime`, and the QVM sees the boundary condition for the full
duration of the delay.

### Refined Fix

Cap changed from `cl.snap.serverTime` to `cl.snap.serverTime - 1`:

```c
// Before:
if ( cl.serverTime > cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime;

// After:
if ( cl.serverTime >= cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime - 1;
```

`cg.time` is now always strictly less than `cg.nextSnap->serverTime`.  The QVM never
triggers a premature snapshot transition regardless of jitter, rate changes, or frame spikes.

The fI ceiling at the cap becomes `(snapshotMsec - 1) / snapshotMsec`:

| Rate | snapshotMsec | fI ceiling |
|------|-------------|------------|
| 60 Hz | 16 ms | 0.9375 |
| 20 Hz | 50 ms | 0.9800 |

Both are well within the INTERP range and imperceptible in practice.

1. **Lagometer top bar = QVM's view of `frameInterpolation`.** If the top bar flickers
   at 60Hz but not at 20Hz, the interpolation ratio is crossing 1.0 near every
   snapshot boundary.  The window is too narrow to tolerate any drift.

2. **The net monitor widget is the right tool for timing correlation.**  It reports the
   engine-side `cl.serverTime` / `cl.frameInterpolation` estimate in real time,
   making the invisible (timing arithmetic) visible alongside the QVM rendering.

3. **At 60Hz, 2ms = 12%.** A drift that was sub-perceptible at 20Hz becomes a clearly
   visible pop at 60Hz because the same absolute jitter represents a much larger
   fraction of the interpolation window.

4. **Tight serverTime cap is free headroom against overshoot.**  Holding `cl.serverTime`
   at `cl.snap.serverTime` costs at most one render frame of position hold (16ms at
   60Hz) in exchange for eliminating all position overshoot.  That tradeoff is
   always worth it until the full Ghidra patch trio is applied.
