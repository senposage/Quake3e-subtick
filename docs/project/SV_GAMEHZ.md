# sv_gameHz — Legacy Reference

> **UT 4.3.4 with `sv_antiwarp 2`:** Use `sv_gameHz 0` (default). This document is only relevant for legacy setups (UT 4.0-4.2 or QVM g_antiwarp).

---

## What sv_gameHz Does

Decouples the QVM game frame rate from the engine tick rate:

- `sv_fps` = engine ticks/sec (input sampling, antilag, snapshots)
- `sv_gameHz` = QVM `GAME_RUN_FRAME` rate (what `level.time` sees)

**sv_gameHz > 0:** Inner loop in `SV_Frame()` accumulates `sv.gameTimeResidual` and fires `GAME_RUN_FRAME(sv.gameTime)` at `sv_gameHz` Hz. Between firings, `sv.gameTime` lags `sv.time` — this gap drives the position extrapolation in `SV_BuildCommonSnapshot`.

**sv_gameHz 0 (disabled):** Effective rate = sv_fps. `GAME_RUN_FRAME` fires every engine tick. `sv.gameTime == sv.time` always. No extrapolation gap exists.

---

## Version Compatibility Matrix

| Target | sv_gameHz | Antiwarp | Notes |
|--------|-----------|----------|-------|
| **UT 4.3.4 + sv_antiwarp** | **0** | `sv_antiwarp 2` | Recommended. Engine antiwarp is frame-rate aware. |
| UT 4.3.4 + QVM g_antiwarp | 20 | `g_antiwarp 1` | Required. QVM hardcodes `serverTime += 50` (50ms game frames). |
| UT 4.0-4.2 | 20 | `g_antiwarp 1` or off | Required. Game code broadly intolerant of non-20Hz rates. |

---

## QVM Antiwarp Constraint

All UT versions (4.0 through 4.3.4) contain a hardcoded antiwarp injection in `g_active.c:G_RunClient`:

```c
ent->client->pers.cmd.serverTime += 50;  // assumes 50ms game frames
```

This fires when `level.time - lastCmdTime > g_antiWarpTol` (default 50ms). It works correctly **only** when game frames are exactly 50ms (sv_gameHz 20).

**At sv_gameHz 0 with sv_fps 60:** Game frames are 16ms but the injection adds 50ms — a 50ms jump fires every 16ms tick, teleporting lagging players at 3× the intended rate. **Never use QVM g_antiwarp with sv_gameHz 0 unless sv_fps is also 20.**

Engine `sv_antiwarp` eliminates this constraint by using `gameMsec` (actual frame duration) instead of a hardcoded 50ms.

---

## Position Correction at sv_gameHz 20

When sv_gameHz 20 and sv_fps 60, `BG_PlayerStateToEntityState` (which stamps entity positions into snapshots) only runs at 20Hz inside `ClientEndFrame`. Three consecutive 60Hz snapshots carry identical positions for **all** players → visible stutter.

### sv_extrapolate (essential at sv_gameHz > 0)

Corrects stale positions in `SV_BuildCommonSnapshot`:
- **Real players:** reads actual `ps->origin` from playerState (updated every usercmd by Pmove — always fresh).
- **Bots:** velocity extrapolation `trBase += ps->velocity * (sv.time - sv.gameTime) * 0.001`. Accurate because bot AI only changes direction at game frame boundaries.

At sv_gameHz 0: no-op (positions are already fresh every tick).

### sv_bufferMs (useful at sv_gameHz > 0)

Per-client ring buffer provides position delay for smoothing between 20Hz updates. Auto mode (`-1`) computes `1000/sv_fps` — one snapshot interval.

| sv_fps | Auto bufMs |
|--------|-----------|
| 20     | 50ms      |
| 40     | 25ms      |
| 60     | 16ms      |
| 80     | 12ms      |
| 100    | 10ms      |
| 125    | 8ms       |

At sv_gameHz 0: not useful (adds latency for no benefit — positions already fresh).

### sv_smoothClients (valuable at any sv_gameHz)

Changes trajectory type from TR_INTERPOLATE to TR_LINEAR. At sv_gameHz > 0, this helps cgame evaluate position continuously between stale snapshots. At sv_gameHz 0, the trajectory type change itself is the value — not the position correction.

---

## Feature Relevance Matrix

| Feature | sv_gameHz 20 | sv_gameHz 0 |
|---------|-------------|-------------|
| sv_extrapolate ps->origin | **Essential** — fixes 20Hz stale positions | No-op — positions update every tick |
| sv_smoothClients TR_LINEAR | Valuable — continuous eval between 20Hz frames | Valuable — trajectory type for cgame |
| sv_bufferMs ring buffer | Useful — smooths 20Hz position gaps | Adds latency for no benefit |
| sv_velSmooth velocity avg | Useful with sv_smoothClients 1 | Marginal — positions already fresh |
| sv_antiwarp (engine) | Works at any sv_gameHz | Works — the whole point |
| QVM g_antiwarp | **Requires sv_gameHz 20** | Broken at sv_fps != 20 |

---

## Configuration Profiles

### UT 4.3.4 — Recommended
```
sv_gameHz 0
sv_fps 60
sv_antiwarp 2
sv_antiwarpDecay 150
sv_extrapolate 1
sv_smoothClients 1
sv_bufferMs 0
sv_velSmooth 32
```

### UT 4.3.4 — With QVM Antiwarp
```
sv_gameHz 20
sv_fps 60
sv_antiwarp 0
g_antiwarp 1
sv_extrapolate 1
sv_smoothClients 1
sv_bufferMs -1
sv_velSmooth 32
```

### UT 4.0-4.2
```
sv_gameHz 20
sv_fps 60
sv_antiwarp 0
sv_extrapolate 1
sv_smoothClients 1
sv_bufferMs 0
```

---

## Combination Table (sv_gameHz 20)

At sv_gameHz 20, the smoothing pipeline matters because positions are stale between game frames:

| sv_smoothClients | sv_bufferMs | sv_velSmooth | Position Source | Trajectory | Description |
|---|---|---|---|---|---|
| 0 | 0 | any | Current ps->origin | TR_INTERPOLATE | Default. Lowest latency. |
| 0 | -1 | any | Delayed (auto) | TR_INTERPOLATE | One-interval delay for stability. |
| 1 | 0 | 0 | Current ps->origin | TR_LINEAR | Continuous eval, raw velocity. |
| 1 | 0 | 1-100 | Current ps->origin | TR_LINEAR | Continuous + smoothed velocity. |
| 1 | -1 | 1-100 | Delayed (auto) | TR_LINEAR | Best smoothness. Delayed base + smoothed velocity. |
