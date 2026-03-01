# Patch breakout

Standalone patches against [ftwgl/Quake3e](https://github.com/ftwgl/Quake3e) `master`.

Apply in order with `git am patches/*.patch` or `patch -p1 < patches/NNNN-*.patch`.

---

1. **`0001-server-frame-loop.patch`** — Dual-rate `sv_fps`/`sv_gameHz` frame loop decoupling,
   per-tick snapshot dispatch, engine-side antilag (`sv_antilag.c`/`.h`), bot frame clock fix,
   `sv_snapshotFps -1` sentinel that live-tracks `sv_fps`, `sv_busyWait` precise timing,
   `SV_MapRestart_f` game clock sync, `SV_TrackCvarChanges` residual clamping on fps change.
   Files: `sv_main.c`, `sv_init.c`, `sv_bot.c`, `sv_ccmds.c`, `sv_antilag.c` (new), `sv_antilag.h` (new).

2. **`0002-sv-extrapolation-smoothing.patch`** — Engine-side player position fixup between game
   frames. Real players: read `ps->origin` directly (post-Pmove, already correct). Bots: velocity
   extrapolation `trBase += trDelta * dt`. `sv_smoothClients` TR_LINEAR injection runs every tick
   (including game-frame ticks) to prevent 50ms-period stutter. `sv_bufferMs` ring-buffer position
   delay, `sv_velSmooth` velocity averaging.
   Files: `sv_snapshot.c`.

3. **`0003-multistep-pmove.patch`** — `sv_pmoveMsec` multi-step `GAME_CLIENT_THINK` clamping so
   Pmove never sees oversized physics steps. Bots excluded. `goto single_call` stall guard for
   intermission/pause. `sv_snapshotFps` policy in `SV_UserinfoChanged` with `-1`/`0`/`>0`
   semantics. QVM cvar guard blocks cgame from resetting `snaps` or `cg_smoothClients`.
   Files: `sv_client.c`.

4. **`0004-client-jitter-tolerance.patch`** — `cl.snapshotMsec` EMA bootstrapped from
   `sv_snapshotFps` in serverinfo. All time-sync thresholds scaled to snapshot interval:
   `RESET_TIME = snapshotMsec*10`, fast-adjust `= snapshotMsec*2`, extrapolation detection
   margin `= snapshotMsec/3`. `serverTime` clamped to `snap.serverTime + snapshotMsec`.
   Timedemo and download throttle use `snapshotMsec` instead of hardcoded 50ms.
   `net_dropsim` flagged `CVAR_CHEAT`.
   Files: `cl_cgame.c`, `cl_input.c`, `cl_parse.c`, `client.h`, `net_ip.c`.
