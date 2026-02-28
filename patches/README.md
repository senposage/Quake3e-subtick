# Patch breakout

This directory breaks the networking/movement work into standalone patch files:

1. `0001-sv-extrapolation.patch` — sv_extrapolate behavior and related snapshot plumbing (`cbf619e`).
2. `0002-snapshot-dispatch-fixes.patch` — snapshot dispatch timing fixes in server frame loop (`28a1a8c`).
3. `0003-multistep-pmove.patch` — multi-step server-side pmove clamping (`115407a`).
4. `0004-client-jitter-tolerance.patch` — client-side jitter tolerance guard against QVM cvar overrides (`0945fd5`).

Generated with `git format-patch` from their original commits.
