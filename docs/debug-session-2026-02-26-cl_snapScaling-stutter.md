# Debug Session: cl_snapScaling Stutter Investigation
**Date:** 2026-02-26 23:48:52  
**Participants:** senposage, GitHub Copilot  
**Starting commit:** `92835a1ea5477a7834183246ba8d0d0b730d0530`  
**Known-good commit:** `5c4da04256b7beb5dba9a1f3580f635a64e64e59`

---

## Problem Statement

Movement stutter observed when driving `sv_snapshotFps` up, most noticeable on the player's **own movement**. This was the primary flag that pointed to client-side time sync as the issue (own movement uses client prediction via usercmd replay, NOT the snapshot entity interpolation pipeline).

## Root Cause: cl_snapScaling Oscillation Loop

The `cl_snapScaling` cvar (added between `5c4da04` and `92835a1`) introduced three modifications to client time sync that created a feedback oscillation:

### 1. ServerTime Clamp (cl_cgame.c:1273-1278)
```c
int maxTime = cl.snap.serverTime + cl.snapshotMsec;
if ( cl.serverTime > maxTime ) {
    cl.serverTime = maxTime;
}
```
At 1:1 `sv_fps:sv_snapshotFps` ratio, `snapshotMsec` equals the tick interval exactly. The clamp target `snap.serverTime + snapshotMsec` is literally where the next snapshot lands — **zero headroom**. Any render frame where `cl.serverTime` naturally advances past that point gets yanked back.

### 2. Extrapolation Margin (cl_cgame.c:1282-1290)
```c
int extrapMargin = cl.snapshotMsec / 3;  // = 5ms at 16ms windows
if ( cls.realtime + cl.serverTimeDelta - cl.snap.serverTime >= -extrapMargin ) {
    cl.extrapolatedSnapshot = qtrue;
}
```
At `snapshotMsec = 16`, `extrapMargin = 5ms`. This triggers `extrapolatedSnapshot` when `cl.serverTime` is within 5ms of the snapshot boundary — which at 1:1 ratio happens nearly every frame.

### 3. Aggressive Pullback (cl_cgame.c:1089-1090)
```c
cl.serverTimeDelta -= ( cl.snapshotMsec < 30 ) ? 4 : 2;
```
At `snapshotMsec = 16`, pullback is `-4ms` per snapshot — that's 25% of the interpolation window.

### The Death Spiral
1. `cl.serverTime` advances toward `snap.serverTime + 16` (the boundary)
2. At 5ms before boundary, `extrapolatedSnapshot` fires
3. Next snapshot arrives: `serverTimeDelta -= 4` (yanks back 4ms)
4. Now `cl.serverTime` is 4ms behind → pushes forward at `+1ms/snapshot` for 4 snapshots
5. During those 4 recovery snapshots, render frames advance `cl.serverTime` toward boundary again
6. By recovery time, hits the margin again → cycle repeats
7. Oscillation period ≈ 5 snapshots → at 60fps = ~80ms visible rhythmic stutter

### Why Fractional Ratios Helped (But Didn't Fix)
- At 3/4 ratio (e.g., `sv_fps 60 / sv_snapshotFps 45`): `snapshotMsec ≈ 22ms`, wider window reduces clamp hits. Stutter "on the edge of intolerable."
- At 1/4 ratio (e.g., `sv_fps 60 / sv_snapshotFps 15`): `snapshotMsec ≈ 66ms`, huge window, clamp almost never fires. Stutter "greatly reduced."
- At 1:1 ratio: zero slack, constant oscillation.

## Testing Results

| Test | sv_fps | sv_snapshotFps | cl_snapScaling | Result |
|------|--------|---------------|----------------|--------|
| 1 | 60 | 60 | 1 | **Stutter** — 1:1 ratio, oscillation loop |
| 2 | 60 | 45 (3/4) | 1 | Better but "on the edge of intolerable" |
| 3 | 60 | 15 (1/4) | 1 | "Greatly reduced" but still present |
| 4 | 60 | 60 | 0 | **Fixed** — vanilla thresholds, no clamp |
| 5 | 125 | 125 | 0 | "Better but still wrong" — vanilla `-2ms` pullback is 25% of 8ms window |
| 6 | 60 | 60 | 0 (reload) | Reload didn't fix → not EMA convergence issue |

### Key Observation
`cl_snapScaling 0` fixed the stutter at 60/60 — confirming the clamp and adaptive thresholds were the primary cause. Residual issues at 125/125 are from vanilla's `-2ms` pullback being relatively aggressive at 8ms windows, but this is much less severe.

## Decision: Remove cl_snapScaling Entirely

**Rationale:**
- The known-good build (`5c4da04`) had NO client-side time sync modifications and worked fine at high snapshot rates
- Vanilla Q3 time sync is battle-tested and self-regulating:
  - `+1ms/-2ms` drift keeps `cl.serverTime` naturally ~3ms behind the snapshot boundary
  - 500ms reset / 100ms fast adjust are conservative but don't cause stutter
  - 5ms extrapolation margin at 16ms windows triggers ~30% of the time → gentle pullback
- The `frameInterpolation > 1.0` overshoot the clamp was trying to prevent:
  - Doesn't affect own movement (uses client prediction, not interpolation)
  - For other entities: single-frame overshoot at 60fps is sub-pixel, imperceptible
- The "fix" was worse than the "problem"

**PR:** Remove cl_snapScaling and serverTime clamp — revert client time sync to vanilla

## Related: sv_smoothClients / sv_bufferMs Composability

### Discovery
During investigation, found that `sv_smoothClients` and `sv_bufferMs` are **mutually exclusive** due to `if / else if` structure in `SV_BuildCommonSnapshot()`. `sv_smoothClients` takes priority and completely ignores `sv_bufferMs`.

### Previous Ring Buffer Testing Was Contaminated
The ring buffer (`sv_bufferMs`) was previously tested WITH `cl_snapScaling 1` active. The client-side oscillation would have made the ring buffer's delayed positions appear worse than they actually are. **All ring buffer results need retesting with `cl_snapScaling` removed.**

### New Architecture: Two-Phase Pipeline
Changed from mutually exclusive to composable:

**Phase 1 — Position Source** (`sv_bufferMs`):
- `sv_bufferMs 0`: Use current position
- `sv_bufferMs -1`: Auto-delay (50 - 1000/sv_fps)
- `sv_bufferMs 1-100`: Manual delay in ms

**Phase 2 — Trajectory Type** (`sv_smoothClients`):
- `sv_smoothClients 0`: TR_INTERPOLATE (client lerps between snapshot positions)
- `sv_smoothClients 1`: TR_LINEAR (client evaluates trajectory continuously)
  - `sv_velSmooth 0`: Raw velocity
  - `sv_velSmooth 1-100`: Averaged velocity from ring buffer

### Full Combination Table

| sv_smoothClients | sv_bufferMs | sv_velSmooth | Position | Trajectory | Description |
|---|---|---|---|---|---|
| 0 | 0 | any | Current | TR_INTERPOLATE | **Default.** Lowest latency. |
| 0 | -1 | any | Delayed (auto) | TR_INTERPOLATE | Stable lerp targets, adds latency. |
| 0 | 1-100 | any | Delayed (manual) | TR_INTERPOLATE | Manual delay control. |
| 1 | 0 | 0 | Current | TR_LINEAR | Continuous trajectory, raw velocity. |
| 1 | 0 | 1-100 | Current | TR_LINEAR | Continuous trajectory, smoothed velocity. |
| 1 | -1 | 0 | Delayed (auto) | TR_LINEAR | **(NEW)** Delayed base + raw velocity extrapolation. |
| 1 | -1 | 1-100 | Delayed (auto) | TR_LINEAR | **(NEW, most promising)** Delayed base + smoothed velocity. |
| 1 | 1-100 | 1-100 | Delayed (manual) | TR_LINEAR | **(NEW)** Same with manual delay. |

### Recommended Test Presets (post-merge)

| Preset | sv_smoothClients | sv_bufferMs | sv_velSmooth | Notes |
|---|---|---|---|---|
| Competitive | 0 | 0 | 0 | Raw positions, minimal processing |
| Balanced | 0 | -1 | 0 | Position delay matches vanilla feel |
| Smoothest | 1 | -1 | 32 | Delayed position + smoothed velocity + TR_LINEAR |

**PR:** Make sv_smoothClients and sv_bufferMs composable instead of mutually exclusive

## TODO After Both PRs Merge

- [ ] Retest `sv_bufferMs -1` without `cl_snapScaling` — was previously contaminated
- [ ] Test `sv_smoothClients 1` + `sv_bufferMs -1` + `sv_velSmooth 32` (the new "best of both worlds" combo)
- [ ] Test at `sv_fps 125 / sv_snapshotFps 125` — vanilla time sync at 8ms windows
- [ ] Test at `sv_fps 60 / sv_snapshotFps 60` — confirm stutter is gone
- [ ] Test fractional ratios again to see if they're now equally smooth
- [ ] Profile ring buffer overhead at high tick rates

## Key Lessons

1. **Own movement stutter = client-side problem.** The entity snapshot pipeline doesn't affect your own predicted movement. If your own movement stutters, look at `cl.serverTime` / `serverTimeDelta` / the time sync loop in `CL_SetCGameTime()`.

2. **Don't fight the vanilla time sync.** Q3's `+1/-2` drift system is elegantly simple and self-correcting. Adding clamps and scaled pullbacks creates feedback loops.

3. **Test combinations independently.** `cl_snapScaling` contaminated all ring buffer test results because the client was oscillating regardless of what the server did.

4. **1:1 sv_fps:sv_snapshotFps is the worst case for timing.** Every tick produces a snapshot, leaving zero slack for delivery jitter. Fractional ratios naturally create buffer.

5. **Server-side smoothing and client-side time sync are independent axes.** They should compose, not conflict.