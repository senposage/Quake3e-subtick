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

**Remaining symptom: intermittent top-line chop in the lagometer.**  
The log did not contain enough data to diagnose it.  The new fields added this session
(see below) will tell us whether it is a client frame-time spike or snap delivery jitter.

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

## The Oscillation: What cg_drawfps Jitter Actually Means

### cg_drawfps is NOT ping — but both oscillate for the same reason

| Display | What it measures | Units |
|---|---|---|
| `cg_drawfps` | Render framerate — `1000 / cls.frametime` (wall-clock time between rendered frames) | frames/second |
| `cl_drawping` | Network RTT — `cls.realtime − cl.outPackets[N].p_realtime` | milliseconds |

During the oscillation episode **both gauges jitter wildly**, but for different immediate reasons:

- **`cg_drawfps` jitters** because `cl.serverTime` (passed as `cg.time` via `CG_DRAW_ACTIVE_FRAME`)
  bounces each frame → cgame does alternating amounts of rendering work → wall-clock frame time
  varies → FPS display oscillates.
- **`cl_drawping` jitters** because the same `cl.serverTime` oscillation corrupts `p_serverTime`
  in outgoing packets → the ping loop matches packet N on one snap, packet N−1 on the next →
  ping alternates between two values (e.g. 32ms / 50ms).

`cg_drawfps` oscillation is the more direct symptom: it means `cl.serverTime` itself is
bouncing. The ping oscillation is a knock-on effect of that same bounce. **Seeing both
oscillate simultaneously confirms the self-sustaining feedback loop is active.**

### Causal chain

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
If ping oscillates by < 32ms → handled only by +1/−2ms slow drift → takes many seconds to
converge → sustained chop without triggering FAST events.

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
