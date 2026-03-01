# Last Session Briefing
**Updated:** 2026-03-01  
**Branch:** `copilot/document-issue-frame-interpolation`

> This file is rewritten at the end of every session.  Read it first.
> Full session history lives in `archive/docs/debug-session-*.md`.

---

## What We Were Working On

Intermittent INTERP/EXTRAP oscillation visible in `cg_lagometer` (top bar flickers
green↔yellow/red) and in the `cl_netgraph` widget (`fI` crossing 1.0).  The lagometer
flicker is the user's primary symptom report.

The oscillation is **intermittent** — often absent for a full 20-minute round — but
reliably inducible by changing `sv_fps` or related cvars mid-session.

---

## Root Cause Found and Fixed

**File:** `code/client/cl_cgame.c` — `CL_SetCGameTime()`

The `cl.serverTime` cap was at exactly `cl.snap.serverTime`.  The QVM transitions its
snapshot window the moment `cg.time >= cg.nextSnap->serverTime`.  Since
`cg.nextSnap->serverTime == cl.snap.serverTime`, the cap was landing directly on the
trigger boundary — the cap itself caused the QVM to advance, find no newer snapshot,
set `cg.nextSnap = NULL`, and enter EXTRAP.

**Fix committed:** cap tightened by 1 ms:

```c
// Before
if ( cl.serverTime > cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime;

// After
if ( cl.serverTime >= cl.snap.serverTime )
    cl.serverTime = cl.snap.serverTime - 1;
```

This keeps `cg.time` strictly below `cg.nextSnap->serverTime` at all times.  The fI
ceiling at the cap is ~0.94 at 60 Hz — imperceptible.

**The fix also explains spontaneous occurrence without cvar changes:** the equilibrium
margin between `cl.serverTime` and the old cap was only ~5 ms.  Any render frame that
runs 5–6 ms longer than average (routine OS jitter) was enough to hit it.

Full write-up: `archive/docs/debug-session-2026-03-01-intermittent-oscillation-diagnostics.md`

---

## Other Changes This Session

### fI widget EMA smoothing (`cl_scrn.c`)
Raw `cl.frameInterpolation` cycles 0→0.94 every 16 ms at 60 Hz — unreadable blur.
Now displays a smoothed EMA (α = 0.2) at `%.2f` precision.  Reads ~0.47 at steady
state (mid-window average) or ~0.94 when the cap is actively holding.

### `cl_netlog 1` now captures timing events (`cl_scrn.c`, `cl_cgame.c`, `client.h`)
Previously `cl_netlog 1` only logged console commands — useless during play.
Now also logs `DELTA FAST` and `DELTA RESET` lines whenever `CL_AdjustTimeDelta`
fires a fast-adjust or reset:

```
[14:32:07] DELTA FAST   dT=-312ms  dd=38ms
[14:33:19] DELTA RESET  dT=-298ms  dd=523ms
```

Log file: `netdebug_YYYYMMDD_HHMMSS.log` in the game folder.
File path is printed to console when first opened.

---

## Current Open Question

**Is the -1 fix sufficient, or is there a second oscillation source?**

The fix addresses the known mechanism.  To confirm it's complete, the user needs to:

1. Enable `cl_netlog 1` before play starts.
2. When/if the oscillation fires again, immediately run `netgraph_dump` in console.
3. Retrieve `netdebug_*.log` from the game folder and share it.

**What the log will tell us:**
- **No `DELTA` lines near the oscillation** → fix was complete; done.
- **`DELTA FAST` line just before oscillation** → a network disturbance pushed
  `serverTimeDelta` high enough that the -1 margin wasn't enough; need to widen margin
  or review `fastAdjust` threshold.
- **No `DELTA` lines but oscillation still occurs** → a different, slower mechanism is
  at work below the fast-adjust threshold; next step is per-snapshot `serverTimeDelta`
  logging.

---

## Diagnostic Quick Reference

| Tool | How to activate | What it shows |
|------|----------------|---------------|
| `cl_netgraph 1` | in-game cvar | Live overlay: snap Hz, ping, smoothed fI, dT, drop, in/out, seq |
| `cl_netlog 1` | in-game cvar | Continuous log: console cmds + FAST/RESET delta events |
| `cl_netlog 2` | in-game cvar | Level 1 + per-second stats (fI, dT, ping, rates) |
| `netgraph_dump` | console command | Point-in-time: full timing state + all server cvars |

Log location: **game folder** → `netdebug_YYYYMMDD_HHMMSS.log`

---

## Branch / Commit State

All changes are on `copilot/document-issue-frame-interpolation`.  Latest commits:

| Commit | Summary |
|--------|---------|
| `eeeaead` | Fix intermittent EXTRAP: cap serverTime at snap.serverTime - 1 |
| `dff947f` | Smooth fI widget display with EMA; add cl_netlog timing event capture |
| `d96e6f1` | Document intermittent oscillation session (this doc's source) |

---

## Key Files for This Work

| File | Role |
|------|------|
| `code/client/cl_cgame.c` | serverTime cap, extrapolation detection, AdjustTimeDelta |
| `code/client/cl_scrn.c` | net monitor widget, log hooks, netgraph_dump |
| `code/client/cl_parse.c` | snapshotMsec EMA, bootstrap from sv_snapshotFps |
| `code/client/client.h` | clientActive_t fields, SCR_* declarations |
| `archive/docs/frameinterpolation-clamp-engine-fix.md` | Full cap history and rationale |
| `archive/docs/debug-session-2026-03-01-frameinterpolation-lagometer.md` | Original fI session |
| `archive/docs/debug-session-2026-03-01-intermittent-oscillation-diagnostics.md` | This session's full write-up |
