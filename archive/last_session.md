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
           drop=0/s  in=4677B/s  out=4163B/s  ft=14/16/23ms  snapgap=1/2ms  caps=1  extrap=3
```

| Field | Meaning | What a bad value tells us |
|-------|---------|--------------------------|
| `ping=avg(min..max)` | RTT per-snap: average and jitter range | Wide spread (e.g. `32(18..52)`) = network RTT jitter; suspect bufferbloat, WiFi, or ISP |
| `dT=min..max` | serverTimeDelta range over the second | Wide range → brief dT spike invisible in old point-in-time sample |
| `ft=min/avg/max` | Client frame time min / average / peak (ms) | `max` spike → OS jitter; very low `min` alongside high `max` = occasional freeze, not sustained |
| `snapgap=avg/max` | Snap arrival interval deviation: average and peak | High `avg` = sustained delivery jitter (network); single `max` spike with low `avg` = one late snap |
| `caps=N` | Times the `-1ms` serverTime cap fired | Nonzero + `ftmax` spike = client frame caused the boundary hit |
| `extrap=N` | Frames where `extrapolatedSnapshot` set | Expected nonzero (normal drift control); zero = very healthy |

### Event lines (`cl_netlog 1`)

```
[01:13:55] SNAP LATE  +12ms  (expected 16ms  got 28ms)   ← network-side: snap 1.5× late
[01:14:32] DELTA FAST  dT=-21192ms  dd=8ms               ← existing
[01:14:32] DELTA RESET  dT=-21200ms  dd=500ms             ← existing
```

`SNAP LATE` fires whenever a snap arrives more than 1.5× the expected interval. This is a direct timestamp for the network-side cause of top-line chop, without needing level-2 logging.

**Files changed this session:** `cl_scrn.c`, `cl_cgame.c`, `cl_parse.c`, `client.h`

---

## What to Look for Next Time the Chop Occurs

Enable `cl_netlog 2` before play.  When chop is seen, look at the log around that time:

| What you see | Cause | Next step |
|---|---|---|
| `SNAP LATE` event + high `snapgap avg` | Sustained network jitter; snap arrived late | Check path jitter / server tick consistency |
| `SNAP LATE` event + low `snapgap avg` | One-off late snap (single burst loss) | Check for packet loss (`drop=N`) |
| Wide `ping` spread (e.g. `32(18..52)`) | RTT jitter (WiFi, bufferbloat, ISP) | Use wired connection; check QoS/router |
| `caps=N` (N>0) + `ft` max spike | Client frame too long — hit snap boundary | Investigate OS scheduler / vsync / frame limiter |
| `caps=N` (N>0), no `ft` spike | dT drift alone hit the boundary | Check `dT` range — wider than ±1ms? |
| None of the above | Something else | Next step: add per-frame serverTimeDelta logging |

---

## Diagnostic Quick Reference

| Tool | How to activate | What it shows |
|------|----------------|---------------|
| `cl_netgraph 1` | in-game cvar | Live overlay: snap Hz, ping, smoothed fI, dT, drop, in/out, seq |
| `cl_netlog 1` | in-game cvar | Continuous log: cmds + FAST/RESET delta events + **SNAP LATE events** |
| `cl_netlog 2` | in-game cvar | Level 1 + per-second STATS: `ping jitter`, `dT range`, `ft jitter`, `snapgap`, `caps`, `extrap` |
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
