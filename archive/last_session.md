# Last Session Briefing
**Updated:** 2026-03-01  
**Branch:** `copilot/analyze-netlog-data`

> This file is rewritten at the end of every session.  Read it first.
> Full session history lives in `archive/docs/debug-session-*.md`.

---

## Status

**INTERP/EXTRAP oscillation: FIXED.**  
User shared a netlog confirming zero EXTRAP events across the entire session.
The `snap.serverTime - 1` cap is working; fI peaks at 0.938 (= 15/16) and never crosses 1.0.

**½ms accumulator: PARTIALLY FIXED — 2+2 batch problem identified and fixed.**  
The accumulator reduced episode severity but created constant low-level oscillation
(every 4 snaps ≈ 64ms period).  Fix 2 (fold `slowFrac` into the extrap condition in ¼ms
units) eliminates the binary 2+2 batch pattern, producing true 1+1 alternation and a
constant serverTimeDelta.  Awaiting confirmation.

**Netgraph slow counter display: FIXED.**  
Changed from absolute count (always cycling 0→30, always yellow) to signed net drift
(abs(up-commits − down-commits) per second).  At equilibrium, net = 0 → green.

**Remaining symptom: intermittent top-line chop in the lagometer.**  
Expected to be resolved by Fix 2 (constant serverTimeDelta → no ping jitter).

---

## What the Netlog Showed

Session ran at 125 Hz then switched to 62 Hz via `\rcon sv_fps 60`.

| Phase | snap | fI range | dT swing | EXTRAP |
|-------|------|----------|----------|--------|
| 125 Hz | 8 ms | 0.500–0.625 | ±1 ms | 0 |
| 62 Hz (drifting) | 16 ms | 0.000–0.938 | 9 ms | 0 |
| 62 Hz (settled) | 16 ms | 0.125–0.688 | ±1 ms | 0 |

Key observations:
- **No DELTA FAST or RESET events** logged — `CL_AdjustTimeDelta` was never triggered.
- **fI=0.938** confirms the `-1` cap firing at 62 Hz ceiling (15/16 = 0.9375). ✓
- **Settled 62 Hz oscillation amplitude is 10× larger than ±1 ms dT drift** — structural
  beat between render frame rate and snap rate, not a timing bug.
- The top-line chop is unrelated to EXTRAP.  The log had no frame-time or snap-jitter
  data, so the cause remains unknown.

---

## Diagnostic Fields in the Log

### Per-second STATS line (`cl_netlog 2`)

```
[HH:MM:SS] STATS  snap=62Hz  ping=32(28..38)ms  fI=0.625(INTERP)  dT=-21189..-21188ms
           drop=0/s  in=4677B/s  out=4163B/s  ft=14/16/23ms  snapgap=1/2ms
           caps=1  extrap=3  fast=0  reset=0
```

| Field | Meaning | What a bad value tells us |
|-------|---------|--------------------------|
| `ping=avg(min..max)` | RTT per-snap: average and jitter range | Wide spread (e.g. `32(18..52)`) = RTT jitter; suspect bufferbloat, WiFi, or ISP |
| `dT=min..max` | serverTimeDelta range over the second | Wide range → brief dT spike invisible in a point-in-time sample |
| `ft=min/avg/max` | Client frame time min / average / peak (ms) | `max` spike → OS jitter; very low `min` + high `max` = occasional freeze, not sustained |
| `snapgap=avg/max` | Snap arrival interval deviation: average and peak | High `avg` = sustained delivery jitter; single `max` spike + low `avg` = one late snap |
| `caps=N` | Times the `-1ms` serverTime cap fired | Nonzero + `ftmax` spike = client frame caused the boundary hit |
| `extrap=N` | Frames where `extrapolatedSnapshot` set | Expected nonzero (normal drift control) |
| `fast=N` | FAST adjustments per second | **Key oscillation indicator** — nonzero most seconds = sustained serverTimeDelta oscillation |
| `reset=N` | RESET adjustments per second | >0 = large sudden dT shift (>500ms); typically a one-off event |
| `slow=N` | abs(up-commits − down-commits) per second | 0 at equilibrium (green); sustained non-zero = serverTimeDelta drifting one direction |

### Event lines (`cl_netlog 1`)

```
[01:13:55] PING JITTER  32ms->50ms  (+18ms)   ← ping jumped 18ms this snap
[01:13:55] PING JITTER  50ms->32ms  (-18ms)   ← and back the next snap  ← oscillation signature
[01:13:55] SNAP LATE  +12ms  (expected 16ms  got 28ms)
[01:14:32] DELTA FAST  dT=-21192ms  dd=32ms
[01:14:32] DELTA RESET  dT=-21200ms  dd=500ms
```

`PING JITTER` fires per-snap when `|ping − prevPing| ≥ max(snapshotMsec/2, 10ms)`.
An alternating `+N / −N` pattern on consecutive lines is the **oscillation signature** — see below.

---

## The Oscillation: Root Cause — Confirmed

### Evidence summary

| Source | Value | Conclusion |
|---|---|---|
| `cg_drawfps` ping display | oscillates 32↔40ms per snap | client-side measurement artifact |
| `cg_drawfps` FPS display | **stable** (no oscillation) | render loop is fine |
| Scoreboard ping | steady 40–41ms | server-reported average is accurate |
| ICMP ping to server (Windows) | 38–44ms, stable | **network is fine — oscillation is 100% client-side** |
| Effects observed | mild lag + visible position jitter from other clients | `cl.serverTime` itself is oscillating |

### Root cause: slow-path ±1ms oscillation in `CL_AdjustTimeDelta`

The slow drift path made integer ±1ms adjustments to `serverTimeDelta` every snap.  At the
60Hz equilibrium (50% extrapolation rate) the result was `serverTimeDelta` toggling ±1ms
*every single snap*, causing `cl.serverTime` to oscillate by ±1ms.

**Full causal chain:**
```
CL_AdjustTimeDelta slow path: +1ms / -1ms per snap at 60Hz equilibrium
    → serverTimeDelta oscillates ±1ms every snap
        → cl.serverTime = cls.realtime + serverTimeDelta  oscillates ±1ms
            → outgoing commands stamped with oscillating serverTime
                → server Pmove runs alternating time steps
                    → position jitter visible from other clients       ← gameplay impact
            → client-side prediction uses oscillating time base
                    → mild lag / stutter felt locally                  ← gameplay impact
            → p_serverTime in outgoing packets oscillates ±1ms
                → ping loop straddles commandTime boundary
                    → matched packet alternates between i and i+1
                        → ping display flips 32↔40ms per snap          ← observable symptom
```

The 10–20 second envelope (starts slow, ramps up, levels out, returns) is how long
`serverTimeDelta`'s equilibrium point spends within ±1ms of the `commandTime =
p_serverTime[i]` boundary, driven by very slow server-clock drift (~0.003Hz off nominal
60Hz → 1ms drift per ~330 snaps ≈ 5s per ms).  When the equilibrium drifts clear of the
boundary the ping display stabilises on its own — but the ±1ms cl.serverTime oscillation
and its gameplay effects continue regardless.

### Fix 1: ½ms fractional accumulator in `CL_AdjustTimeDelta` (cl_cgame.c)

Replace the integer ±1ms per-snap step with a ½ms accumulator (4 units = 1ms).  At
exactly 50% extrapolation rate the accumulator oscillates 0↔2, never reaching the ±4
commit threshold — `serverTimeDelta` stays perfectly constant.

```
Equilibrium (50% extrap, alternating no/extrap):
  slowFrac: 0 → +2 → 0 → +2 → 0 …   (never reaches ±4)
  serverTimeDelta: CONSTANT            ← no more ±1ms noise
```

Off-equilibrium convergence speed is halved (e.g. a 5ms excursion now takes ~167ms to
correct via the slow path instead of ~83ms), but large excursions still hit the FAST or
RESET paths which are unchanged.

`slowFrac` is reset to 0 whenever FAST or RESET fires so stale slow-drift history never
corrupts recovery from a large correction.

### Why Fix 1 alone was insufficient — the 2+2 batch problem

The ½ms accumulator assumed the extrap condition would produce **1+1 alternating** snaps
(extrap, non-extrap, extrap …) so `slowFrac` oscillates 0→-2→0→-2 and never hits ±4.

In practice the extrap condition is **binary per integer-ms serverTimeDelta level**: at
`serverTimeDelta = N` ALL snaps fire extrap; at `N-1` NONE do.  Snaps therefore arrive in
**2+2 batches** (2 extrap → slowFrac -4 → commit; 2 non-extrap → +4 → commit), producing
a constant ±1ms serverTimeDelta oscillation at ~30 commits/second.

Observed in logs: `dT=143378..143379ms` (1ms range), `PING JITTER ±16ms` on every other
snap, `slow≈30/s`, `fast=0`.  The ½ms accumulator **traded** rare-but-long oscillation
episodes for a **constant** low-level oscillation — less severe per episode, but
continuous and still causing feelable lag.

### Fix 2: fold `slowFrac` into the extrap condition (¼ms precision)

Move `slowFrac` to file scope so `CL_SetCGameTime()` can include it in the extrapolation
check, evaluated in ¼ms units:

```c
if ( ( cls.realtime + cl.serverTimeDelta - cl.snap.serverTime ) * 4 + slowFrac
         >= -( extrapolateThresh * 4 ) )
```

**Why this eliminates the 2+2 problem:**  At the exact equilibrium threshold
`diff = -extrapolateThresh` the condition becomes `slowFrac >= 0`:

```
Equilibrium (slowFrac visible to condition):
  Snap 1: slowFrac=0  → 0 ≥ 0  → YES (extrap)  → slowFrac=-2
  Snap 2: slowFrac=-2 → -2 ≥ 0 → NO  (no extrap) → slowFrac=0
  Snap 3: slowFrac=0  → YES → slowFrac=-2
  Snap 4: slowFrac=-2 → NO  → slowFrac=0  …
  → perfect 1+1 alternation, slowFrac never reaches ±4, no commits
  → serverTimeDelta CONSTANT, ping stable, no feelable lag
```

Off-threshold convergence still works correctly:
- `diff = -thresh + 1ms` → condition true for both slowFrac=0 and -2 → 2 consecutive
  extrap → commit (serverTimeDelta--)  → back to threshold → stabilises ✓
- `diff = -thresh - 1ms` → condition false for both → 2 non-extrap → commit (++) ✓

### cg_drawfps shows FPS and ping — distinct displays

| Display | What it measures | Units |
|---|---|---|
| `cg_drawfps` FPS | `1000 / cls.frametime` (wall-clock between rendered frames) | frames/second |
| `cg_drawfps` ping | `snapshot->ping` = raw per-snap `cls.realtime − p_realtime[matched_packet]` | milliseconds |

The FPS display was stable because the render loop is driven by `cls.frametime` (wall
clock), not by `cl.serverTime`.  The ping display oscillated because the ping measurement
is directly derived from `cl.serverTime` (via `p_serverTime` in outgoing packets).

### Causal chain (pre-fix)

```
cl.serverTime oscillates
    → p_serverTime in outgoing command packets oscillates
        → ping loop matches different packet (N vs N-1) on alternate snaps
            → ping alternates between two values (e.g. 32ms / 50ms)
                → newDelta = snap.serverTime - cls.realtime alternates
                    → CL_AdjustTimeDelta fires FAST alternately +/−
                        → serverTimeDelta oscillates
                            → cl.serverTime oscillates   ← feedback loop
```

The FAST threshold at 60Hz is `2 × snapshotMsec = 32ms`.
If ping oscillates by ≥ 32ms → FAST fires every other snap → `fast=~30/s` in STATS.
If ping oscillates by < 32ms → handled only by slow drift → sustained chop without
triggering FAST events.

### What the log will show during oscillation

```
[01:13:55] PING JITTER  32ms->50ms  (+18ms)
[01:13:55] PING JITTER  50ms->32ms  (-18ms)
[01:13:55] PING JITTER  32ms->50ms  (+18ms)
[01:13:55] PING JITTER  50ms->32ms  (-18ms)
[01:13:56] STATS  ...  ping=41(32..50)ms  dT=-21170..-21150ms  fast=30  reset=0
```

The alternating sign and the high `fast=` count together confirm the self-sustaining oscillation.

### Breaking the loop

The oscillation is self-sustaining once started. The trigger is whatever first caused
`cl.serverTime` to overshoot a snap boundary. Candidates in order of likelihood:

1. A single `SNAP LATE` event (network) pushed `newDelta` far enough to trigger FAST
2. A single `ft=max` spike (OS frame-time) pushed `cl.serverTime` past the cap
3. A `cl_timeNudge` change or connect/reconnect

---

## What to Look for Next Time the Chop Occurs

Enable `cl_netlog 2` before play. When chop is seen, look at the log around that time:

| What you see | Diagnosis | Action |
|---|---|---|
| Alternating `PING JITTER +N/-N` + `fast=~30/s` | Self-sustaining oscillation (see above) | Find what triggered it: look for a `SNAP LATE` or `ft max` spike just before |
| Single `SNAP LATE` then oscillation starts | Network spike triggered the loop | Check path jitter / server tick consistency |
| `ft max` spike then oscillation starts | OS frame-time spike triggered the loop | Investigate OS scheduler / vsync / frame limiter |
| `fast=0 reset=0`, wide `ping` spread | RTT jitter in slow-drift zone (< 32ms swing) | Check bufferbloat/WiFi; may need lower sv_fps |
| `caps=N` + `ft max` spike, no oscillation | One-off cap hit (frame too long) | Single event, not a loop; check frame pacing |
| None of the above | Unknown | Next step: per-frame serverTimeDelta logging |

---

## Diagnostic Quick Reference

| Tool | How to activate | What it shows |
|------|----------------|---------------|
| `cl_netgraph 1` | in-game cvar | Live overlay: snap Hz, ping, smoothed fI, dT, drop, in/out, seq |
| `cl_netlog 1` | in-game cvar | Events: FAST/RESET deltas + SNAP LATE + **PING JITTER** |
| `cl_netlog 2` | in-game cvar | Level 1 + per-second STATS: all jitter fields + `fast`/`reset` counts |
| `netgraph_dump` | console command | Point-in-time: full timing state + all server cvars |

Log location: **game folder** → `netdebug_YYYYMMDD_HHMMSS.log`

---

## Key Files

| File | Role |
|------|------|
| `code/client/cl_cgame.c` | serverTime cap, extrapolation detection, AdjustTimeDelta |
| `code/client/cl_scrn.c` | net monitor widget, log hooks, netgraph_dump, STATS line |
| `code/client/cl_parse.c` | snapshotMsec EMA, snap-interval measurement + jitter hook |
| `code/client/cl_main.c` | per-frame frametime hook |
| `code/client/client.h` | SCR_* declarations |
