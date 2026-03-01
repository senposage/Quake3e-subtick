# Debug Session: Intermittent INTERP/EXTRAP Oscillation — Diagnosis & Diagnostic Infrastructure
**Date:** 2026-03-01 (continuation of lagometer session)  
**Participants:** senposage, GitHub Copilot  
**Related:** `debug-session-2026-03-01-frameinterpolation-lagometer.md`

---

## Problem Statement

After the initial `cl.snap.serverTime` cap fix (previous session), the INTERP/EXTRAP
oscillation was still occurring — but **intermittently**.  It could run for a nearly
complete 20-minute round without appearing, then suddenly fire.  The user noted it could
usually be induced by changing `sv_fps` or one of the newer cvars.

---

## Refined Root Cause: The Cap Boundary Itself

### Quick answer: yes, this explains spontaneous occurrence without any cvar changes

The cap at exactly `cl.snap.serverTime` had a subtle flaw that caused the very symptom it
was designed to prevent.

**QVM snapshot transition rule:**
```c
// CG_TransitionSnapshot fires when:
if ( cg.time >= cg.nextSnap->serverTime )   // advance the window
```

Since `cg.nextSnap->serverTime == cl.snap.serverTime` (the engine's current snap IS the QVM's
next interpolation target), capping `cl.serverTime` at exactly `cl.snap.serverTime` put
`cg.time` right on the boundary — triggering the transition condition on the same frame
the cap fired.

After the transition, the QVM looked for the snapshot beyond `cl.snap`.  That snapshot had
not arrived yet.  The QVM set `cg.nextSnap = NULL` and entered **EXTRAP mode**.

### Why it was intermittent, not constant

Under steady-state play, the `+1/-1` drift equilibrium holds `cl.serverTime` roughly
5 ms below `cl.snap.serverTime`.  The cap never fires during those periods.

Two things break that margin without any cvar change:

1. **Normal render-frame variance** — any render frame that runs 5–6 ms longer than
   average (OS scheduler jitter, vsync alignment, background process) pushes
   `cl.serverTime` from ~5 ms behind straight into the cap.  At 60 Hz snaps (16 ms
   windows), a single 5 ms frame spike is enough.  This happens on every PC, constantly,
   but infrequently enough that a full 20-minute round can pass without hitting it.

2. **Drift accumulation timing noise** — the `extrapolatedSnapshot` detection evaluates:
   ```c
   cls.realtime + cl.serverTimeDelta - cl.snap.serverTime >= -extrapolateThresh
   ```
   If a render frame is slightly long on the exact frame the detection should have fired,
   it misses once, `serverTimeDelta` creeps one step too far, and the cap fires on the
   next frame.  Pure statistical noise in frame timing; no cvar required.

### Why `sv_fps` changes make it reliable to induce

When `sv_fps` drops (e.g. 60→20), no new snapshot arrives for roughly 34 ms extra.
During that wait, `cl.serverTime` advances with real time and hits the cap.  Because the
cap sits at exactly `snap.serverTime`, the QVM sees the boundary condition for the full
34 ms gap — the oscillation is sustained for the entire wait rather than just one frame.

This is why the user could "usually induce it" with `sv_fps` changes: that cvar change
makes the intermittent event into a guaranteed, sustained one.

---

## Fix: Tighten Cap by 1 ms

```c
// Before: exact boundary — cap itself triggered QVM transition
if ( cl.serverTime > cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime;

// After: cg.time always strictly < cg.nextSnap->serverTime
if ( cl.serverTime >= cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime - 1;
```

**File:** `code/client/cl_cgame.c` — `CL_SetCGameTime()`

`cg.time` is now always strictly less than `cg.nextSnap->serverTime`.  The QVM's
transition condition (`cg.time >= cg.nextSnap->serverTime`) can never be met while the
engine is holding the cap, regardless of render frame variance, drift noise, or `sv_fps`
changes.

**fI ceiling at the cap** becomes `(snapshotMsec - 1) / snapshotMsec`:

| Rate | snapshotMsec | fI ceiling |
|------|-------------|------------|
| 60 Hz | 16 ms | 0.9375 |
| 20 Hz | 50 ms | 0.9800 |

Both are well within the INTERP range and imperceptible in practice.

---

## fI Widget Readability Improvement

**Problem:** At 60 Hz snaps, `cl.frameInterpolation` cycles from 0.000 → ~0.937 every
16 ms.  At 250 fps display rate, that's roughly 4 display frames per cycle — the `%.3f`
readout was an unreadable blur.

**Fix:** Applied a per-frame EMA (α = 0.2) inside `SCR_DrawNetMonitor`.  The smoothed
value moves at 1/5 the raw rate.

```c
static float smoothFI = 0.0f;
smoothFI = smoothFI * 0.8f + cl.frameInterpolation * 0.2f;
Com_sprintf( line, sizeof(line), "fI:   %.2f INTERP", smoothFI );
```

**Steady-state readout:**
- 60 fps display / 60 Hz snaps: stabilises near **0.47** (mean of the 0→0.94 sawtooth)
- 250 fps display / 60 Hz snaps: stabilises near **0.94** (cap region; update rate faster
  than the sawtooth cycle so EMA stays near the ceiling)

Both readings are meaningful:
- Near 0.47: healthy mid-window average — engine is pacing well
- Near 0.94: cap region active — serverTimeDelta is at or above equilibrium

`%.3f` → `%.2f` also removes the third decimal digit, further reducing visual noise.

---

## Diagnostic Infrastructure Added

### Problem: cl_netlog 1 was effectively silent during play

`cl_netlog 1` previously only captured console `CMD:` lines.  During a session with no
console activity, the log was empty — useless for correlating the oscillation to network
events.

### Fix: SCR_LogTimingEvent() — FAST and RESET events logged at cl_netlog 1

Added a new public hook `SCR_LogTimingEvent()` (declared in `client.h`, implemented in
`cl_scrn.c`) called from `CL_AdjustTimeDelta()` in the FAST-adjust and RESET branches:

```
[14:32:07] DELTA FAST   dT=-312ms  dd=38ms
[14:33:19] DELTA RESET  dT=-298ms  dd=523ms
```

- **`FAST`** — `serverTimeDelta` was displaced by `2×snapshotMsec` or more.  This is the
  event that previously pushed `serverTimeDelta` above its equilibrium and caused the cap
  to fire on subsequent frames.
- **`RESET`** — `serverTimeDelta` was displaced by `10×snapshotMsec` or more.  Indicates
  a large timing break (alt-tab, map change, extreme packet loss).
- **`dT`** — `serverTimeDelta` value after the adjustment.
- **`dd`** — `deltaDelta`: the magnitude of the disturbance that triggered the event.

### Updated cl_netlog levels

| Level | Captures |
|-------|---------|
| 0     | Off |
| 1     | Console CMD lines + FAST/RESET delta events |
| 2     | Level 1 + per-second STATS lines (fI, dT, snap Hz, ping, drop, in, out) |

### cl_netlog file location

The log is written to the **game folder** (same directory as `baseq3/` or the current
fs_game directory), named:

```
netdebug_YYYYMMDD_HHMMSS.log
```

The file is created lazily when the first loggable event occurs (or immediately on
`netgraph_dump`).  The full path is printed to the console when the file is first opened.

### netgraph_dump vs. cl_netlog

| Tool | Best for |
|------|---------|
| `cl_netlog 1` | Continuous timeline of FAST/RESET events — captures brief sub-second oscillation episodes that the per-second stats would miss |
| `cl_netlog 2` | Steady-state overview of snap rate, ping, fI, dT trends every second |
| `netgraph_dump` | Point-in-time full snapshot: timing state + all CS_SERVERINFO server cvars.  Use at the moment of or immediately after an oscillation episode |

**Recommended procedure when the oscillation next occurs:**

1. Enable `cl_netlog 1` at session start (before the oscillation appears).
2. When the oscillation fires, immediately run `netgraph_dump` in console.
3. After the session, retrieve `netdebug_*.log` from the game folder.

The log will show any FAST or RESET event in the seconds before the oscillation, the
`netgraph_dump` will show the exact server config and timing state at that moment.

---

## What the Data Will Confirm

If the -1 fix is complete:
- **No FAST/RESET lines** during the oscillation episode → the cap was the only mechanism;
  the fix eliminated it.

If a different mechanism is active:
- **FAST line immediately before oscillation** → a network disturbance pushed
  `serverTimeDelta` above equilibrium and the new -1 margin wasn't enough.  Next step:
  widen the margin further or examine the fastAdjust threshold.
- **No FAST/RESET but oscillation still occurs** → the oscillation source is below the
  fastAdjust threshold (< 2×snapshotMsec displacement).  Likely the `+1/-1` slow-drift
  cycle itself crossing the threshold for some other reason.  Next step: add per-snapshot
  logging of `serverTimeDelta` to find the exact crossing point.

---

## Key Lessons

1. **"Exact boundary" caps are dangerous.** Capping at exactly a transition threshold
   moves the trigger from "past the boundary" to "at the boundary" — the symptom
   changes from constant to intermittent, not absent.  Always cap 1 unit short.

2. **Intermittent = small margin + normal noise.** An event that only requires 5 ms of
   frame variance to trigger, on a 5 ms equilibrium margin, will fire a few times per
   20-minute session from pure OS scheduling noise.  No bug, no cvar change required.

3. **Inducible by sv_fps change = margin < inter-snap gap.** If changing the snap rate
   reliably induces the symptom, the margin is smaller than the time between snapshots at
   the new rate.  The symptom was already marginal; the cvar change made it guaranteed.

4. **cl_netlog 1 is only useful if it captures events, not just commands.** Timing events
   must be hooked explicitly.  Console-only logging gives a false sense of coverage.
