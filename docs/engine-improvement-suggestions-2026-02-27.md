# Engine Improvement Suggestions (Post-attempt Review)

This note summarizes realistic next steps after reviewing existing project docs and debug logs.

## Bottom line

The engine is already close to the practical ceiling **without UT4.3 cgame source**. The biggest remaining visible issues are now mostly on the cgame side (QVM binary assumptions), not server core architecture.

## Priority 1 — Finish the cgame QVM patch trio

1. Clamp `frameInterpolation` to `[0,1]` in cgame.
2. Make `TR_INTERPOLATE` use `trDelta` for short extrapolation.
3. Add `nextSnap == NULL` fallback to extrapolate instead of freezing/crashing.

Why this matters:
- Current docs identify this as the highest value remaining fix for high-rate snapshot smoothness.
- Server-side smoothing/extrapolation is largely done already; cgame still applies 20Hz-era assumptions.

Risk:
- Medium (binary patching), but docs describe concrete search patterns and fallback safety.

## Priority 2 — Validate “best practical” server presets with hard data

Run a structured benchmark matrix and keep only presets that measurably improve outcomes:
- `sv_fps/sv_snapshotFps`: `60/60`, `90/60`, `125/60`, `125/125`
- `sv_extrapolate`: `1`
- `sv_smoothClients`: `0` and `1`
- `sv_bufferMs`: `0`, `-1`
- `sv_velSmooth`: `0`, `16`, `32`

Track:
- Netgraph jitter percentiles (P50/P95/P99 packet interval)
- Hitreg disagreement rate (attacker POV vs rewind result deltas)
- Visual stutter ratings from demo side-by-side

Goal:
- Replace “feels better” decisions with repeatable data and produce one recommended competitive profile.

## Priority 3 — Add observability for timing/jitter regressions

Expose debug counters (toggle via cvar) so timing regressions are caught quickly:
- `SV_Frame` residual clamp hits
- Number of double-tick `Com_Frame` recoveries
- Snapshot interval histogram per client
- `nextSnap == NULL` frequency seen by client (if available via telemetry)

This won’t directly improve smoothness, but it will prevent future regressions and shorten debug cycles.

## Priority 4 — Narrow anti-lag tuning envelope, then lock defaults

Current antilag design is strong. Remaining optimization should be constrained to a small safe envelope:
- `sv_physicsScale`: compare 2 vs 3 vs 4
- `sv_antilagMaxMs`: validate 150/200/250 under realistic pings

Pick one default for competitive play and document rationale to avoid configuration drift.

## What is likely near hard limit under current constraints

Given constraints documented in the repo:
- `sv_gameHz` effectively locked to `20` due to UT antiwarp assumptions.
- No UT4.3 cgame source means interpolation internals cannot be cleanly fixed in source.
- Most severe server-side blockers for `sv_fps > 20` are already addressed.

So, without full QVM source, **remaining gains are incremental** and mostly from:
- cgame binary patch quality,
- parameter tuning,
- and instrumentation-backed configuration discipline.

## Recommended next action sequence

1. Apply Patch 2 first (frameInterpolation clamp), ship test build.
2. Apply Patch 1 + Patch 3 together, ship second test build.
3. Run controlled benchmark matrix and lock a “competitive default” preset.
4. Keep experimental knobs (`sv_smoothClients`, `sv_bufferMs`) off by default unless data proves benefit.
