# Patch breakout

This directory breaks the networking/movement work into standalone patch files:

1. `0001-sv-extrapolation.patch` — sv_extrapolate behavior and related snapshot plumbing (`cbf619e`).
2. `0002-snapshot-dispatch-fixes.patch` — vanilla `sv_fps` snapshot dispatch timing fixes in server frame loop (`4a1c130`).
3. `0003-multistep-pmove.patch` — vanilla-path multi-step server-side pmove clamping (`7b0f3ca`).
4. `0004-client-jitter-tolerance.patch` — full client jitter-tolerance system on vanilla sv_fps: snapshot interval EMA, scaled time-sync thresholds, extrapolation detection/pullback, serverTime clamp, timedemo scaling, download throttle scaling, and QVM cvar guard (always-on, no deprecated toggle) (`e3aa26d`).

Generated with `git format-patch` from their original commits.
