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

**At sv_gameHz 0 with high sv_fps (e.g. 50):** Game frames are 20ms but the injection adds 50ms — a 50ms jump fires every 20ms tick, teleporting lagging players at 2.5x the intended rate. **Never use QVM g_antiwarp with sv_gameHz 0 unless sv_fps is also 20.**

Engine `sv_antiwarp` eliminates this constraint by using `gameMsec` (actual frame duration) instead of a hardcoded 50ms.

---

## Position Correction at sv_gameHz 20

When sv_gameHz 20 and sv_fps 50, `BG_PlayerStateToEntityState` (which stamps entity positions into snapshots) only runs at 20Hz inside `ClientEndFrame`. Two consecutive 50Hz snapshots carry identical positions for **all** players → visible stutter for any observer.

### Root cause: two distinct staleness problems

**Real players:** `ps->origin` (used by `sv_smoothClients` as the snapshot `trBase`) is only updated when:
1. A usercmd arrives and `GAME_CLIENT_THINK` fires (Pmove runs at client packet rate)
2. `GAME_RUN_FRAME` fires and `G_RunClient` advances `commandTime` to `level.time` (20Hz)

At `sv_gamehz 0`, path (2) fires at 60Hz, keeping `ps->origin` always fresh. At `sv_gamehz 20`, path (2) fires at 20Hz only. Between game frames, if the observed player's packets haven't arrived this tick, `ps->origin` lags `sv.time`. `sv_smoothClients` stamps that stale origin into 2–3 consecutive snapshots → the observer sees freeze-then-jump.

**Bots:** No usercmds at all — `ps->origin` (via `BG_PlayerStateToEntityState`) only updates at 20Hz inside `ClientEndFrame`.

### Fix: per-entity position extrapolation in SV_BuildCommonSnapshot

Both fixes are gated by `sv.time > sv.gameTime` — false at `sv_gamehz 0` where `sv.time == sv.gameTime` always, making both strictly no-ops.

**Real players** (`!usedBuffer`, `!isBot` path):
```c
// ps->commandTime = time of last Pmove (usercmd or G_RunClient)
if ( sv.time > sv.gameTime && ps->commandTime < sv.time ) {
    float dt = min( sv.time - ps->commandTime, sv.time - sv.gameTime ) * 0.001f;
    origin = ps->origin + ps->velocity * dt;   // extrapolate from last Pmove
} else {
    origin = ps->origin;   // already fresh (sv_gamehz 0 or usercmd this tick)
}
```

Uses `ps->commandTime` (not `sv.gameTime`) as the base — more accurate because real players' last Pmove aligns with their usercmd arrival, not necessarily the game-frame boundary.

**Bots** (`!usedBuffer`, `isBot` path):
```c
if ( sv.time > sv.gameTime ) {
    float dt = (sv.time - sv.gameTime) * 0.001f;
    origin = trBase + ps->velocity * dt;   // ps->velocity from last game-frame Pmove
}
```

Uses `sv.gameTime` as the base (bots have no usercmds; their last Pmove was exactly at the game-frame boundary). Keeps `TR_INTERPOLATE` to avoid visual/server mismatch when bots change direction at game-frame boundaries.

### sv_extrapolate (legacy; sv_smoothClients supersedes it)

The engine position corrections above are applied whenever `sv_smoothClients || sv_extrapolate`. `sv_extrapolate` alone (without `sv_smoothClients`) anchors the `TR_INTERPOLATE` trBase but does not provide the `TR_LINEAR` trajectory that allows continuous client-side evaluation. `sv_smoothClients` is required for smooth visuals at any `sv_fps > 20`.

At sv_gameHz 0: both are no-ops for position correction — positions are already fresh every tick.

### sv_bufferMs (useful at sv_gameHz > 0)

Per-client ring buffer provides position delay for smoothing between 20Hz updates. Auto mode (`-1`) computes `1000/sv_fps` — one snapshot interval.

| sv_fps | Auto bufMs |
|--------|-----------|
| 20     | 50ms      |
| 40     | 25ms      |
| 50     | 20ms      |
| 100    | 10ms      |
| 125    | 8ms       |

Only factors of 1000 are listed. Non-factors (e.g. 60: 1000/60=16ms truncated, runs ~62.5Hz) cause server clock drift and should not be used.

At sv_gameHz 0: not useful (adds latency for no benefit — positions already fresh).

### sv_smoothClients (required at sv_fps > 20)

Changes trajectory type from `TR_INTERPOLATE` to `TR_LINEAR`. **Required for smooth visuals at any `sv_fps > 20`** — without it, the client can only lerp between two snapshot positions (instead of evaluating a forward trajectory), and any stale position produces visible freeze-then-jump.

At sv_gamehz > 0: the position extrapolation above ensures the `trBase` in each TR_LINEAR snapshot is a distinct advancing position even between game frames.

---

## Feature Relevance Matrix

| Feature | sv_gameHz 20 | sv_gameHz 0 |
|---------|-------------|-------------|
| sv_smoothClients TR_LINEAR | **Required** — continuous eval; extrapolation fills inter-frame gap | **Required** at sv_fps > 20 — trajectory type for cgame |
| Real-player `ps->origin` extrapolation | **Active** — extrapolates from `ps->commandTime` to `sv.time` | No-op — `ps->commandTime == sv.time` always |
| Bot `trBase` extrapolation | **Active** — extrapolates from `sv.gameTime` to `sv.time` | No-op — `sv.time == sv.gameTime` always |
| sv_bufferMs ring buffer | Useful — smooths 20Hz position gaps | Adds latency for no benefit |
| sv_velSmooth velocity avg | Useful with sv_smoothClients 1 | Marginal — positions already fresh |
| sv_antiwarp (engine) | Works at any sv_gameHz | Works — the whole point |
| QVM g_antiwarp | **Requires sv_gameHz 20** | Broken at sv_fps != 20 |

---

## Configuration Profiles

### UT 4.3.4 — Recommended
```
sv_gameHz 0
sv_fps 50
sv_antiwarp 2
sv_antiwarpDecay 150
sv_extrapolate 1
sv_smoothClients 1
sv_bufferMs 0
sv_velSmooth -1   // auto: 80ms at sv_fps 50
```

### UT 4.3.4 — With QVM Antiwarp
```
sv_gameHz 20
sv_fps 50
sv_antiwarp 0
g_antiwarp 1
sv_extrapolate 1
sv_smoothClients 1
sv_bufferMs -1
sv_velSmooth -1   // auto: 80ms at sv_fps 50
```

### UT 4.0-4.2
```
sv_gameHz 20
sv_fps 50
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
