# UT4.3 Engine Enhancement — Claude Code Context

## Project Overview

Urban Terror 4.3 (UT4.3) server engine enhancements built on top of **Quake3e** (an enhanced Q3 engine). The goal is a competitive-grade dedicated server with:

- `sv_fps 60` (or higher) input sampling rate
- `sv_gameHz 20` game logic rate (locked — see constraints)
- Engine-side antilag with sub-tick position history
- Correct physics and game timer behaviour at elevated tick rates
- Visual smoothness improvements for clients

---

## Repository Layout

```
code/server/          ← all engine changes live here
  sv_main.c           ← frame loop, sv_fps/sv_gameHz decoupling, SV_BotFrame placement
  sv_client.c         ← SV_ClientThink multi-step pmove, sv_pmoveMsec clamping
  sv_init.c           ← cvar registration (sv_gameHz, sv_snapshotFps, sv_busyWait,
                         sv_pmoveMsec, antilag cvars)
  sv_snapshot.c       ← TR_LINEAR_STOP velocity injection for player smoothing
  sv_antilag.c        ← engine-side antilag position history + rewind
  sv_antilag.h        ← antilag interface

UrbanTerror42_Source/mod/code/game/   ← QVM source (4.2, used for reference only)
  g_active.c          ← ClientThink_real, G_RunClient, bleed system, antiwarp
  g_main.c            ← G_RunFrame, ExitLevel, CheckIntermissionExit
  bg_pmove.c          ← Pmove, weaponTime, stamina, recoil — all pml.msec based
  bg_misc.c           ← BG_PlayerStateToEntityState (TR_INTERPOLATE hardcode)
                         BG_PlayerStateToEntityStateExtraPolate (exists, never called)
```

> **Note:** We do NOT have UT4.3 QVM source. All QVM-side issues must be worked around engine-side. The 4.2 source is reference only.

---

## What Prevented sv_fps > 20 in the Original Engine

The original Quake3e codebase (pre-modification) had five distinct blockers when raising `sv_fps` above 20. These are confirmed from git history and explain every design decision made here.

### 1. sv_fps defaulted to 20
```c
sv_fps = Cvar_Get("sv_fps", "20", CVAR_TEMP | CVAR_PROTECTED);
```
The default was intentionally set to match the QVM's assumed frame rate. A range check also existed but was commented out:
```c
//Cvar_CheckRange( sv_fps, "20", "125", CV_INTEGER );
```

### 2. GAME_RUN_FRAME fired at sv_fps rate, passing sv.time directly
```c
// original — inside the sv_fps while loop
VM_Call( gvm, 1, GAME_RUN_FRAME, sv.time );
```
There was no separate `sv.gameTime`. At `sv_fps 60`, `level.time` would advance 16ms per engine tick instead of 50ms. The QVM antiwarp blank command injection `cmd.serverTime += 50` then fires a 50ms ghost usercmd every 16ms of real time — teleporting lagging players at 3× the intended rate. This is the hardest-breaking issue and the primary reason `sv_gameHz` must stay locked at 20.

### 3. No separate game clock — decoupling was impossible
Because `GAME_RUN_FRAME` received `sv.time` directly and there was no `sv.gameTime` / `gameTimeResidual`, there was no mechanism to run the engine tick at one rate and the QVM at another. The two were the same variable.

### 4. Snapshot rate hard-coupled to sv_fps
```c
// original — SV_UserinfoChanged
cl->snapshotMsec = 1000 / sv_fps->integer;

// client rate cap:
else if ( i > sv_fps->integer )
    i = sv_fps->integer;
```
Raising `sv_fps` to 60 would flood clients with 60 snapshots/sec even though game state only updated 20 times/sec. `sv_snapshotFps` decouples this — snapshots send at a high rate using the engine clock while carrying game state that only changes at 20Hz.

### 5. No Pmove step clamping
Without `sv_pmoveMsec`, `SV_ClientThink` passed the raw usercmd delta straight to the QVM. At `sv_fps 60` each delta is ~16ms. Physics steps inside `ClientThink_real` are not normalised, creating movement inconsistency across different client framerates.

---

## Custom CVars

Flags used in code (note: not all are ARCHIVE — antilag cvars are SERVERINFO only and must be set in server.cfg to persist).

| Cvar | Default | Description |
|------|---------|-------------|
| `sv_fps` | `60` | Engine tick / input sampling rate |
| `sv_gameHz` | `20` | QVM GAME_RUN_FRAME rate (locked at 20 — see constraints) |
| `sv_snapshotFps` | `60` | Snapshot send rate to clients |
| `sv_pmoveMsec` | `8` | Max Pmove physics step size (ms) |
| `sv_busyWait` | `0` | Spin last N ms before frame instead of sleeping |
| `sv_antilagEnable` | `1` | Engine antilag on/off |
| `sv_physicsScale` | `3` | Antilag sub-ticks per game frame (position history density) |
| `sv_antilagMaxMs` | `200` | Max rewind window (ms) |

---

## Architecture: Decoupled Tick Rates

### Frame Loop (sv_main.c)

```
Com_Frame (real wall clock)
  └─ SV_Frame(msec)
       sv.timeResidual += msec
       Hard clamp: timeResidual = min(timeResidual, frameMsec*2 - 1)  ← prevents burst on fps change
       
       while timeResidual >= frameMsec (1000/sv_fps):
           timeResidual -= frameMsec
           svs.time += frameMsec
           sv.time  += frameMsec
           
           [antilag: record sv_physicsScale position snapshots here]
           
           gameTimeResidual += frameMsec
           while gameTimeResidual >= gameMsec (1000/sv_gameHz):
               gameTimeResidual -= gameMsec
               sv.gameTime += gameMsec
               SV_BotFrame(sv.gameTime)        ← IMPORTANT: bot AI ticks here, in lockstep
               GAME_RUN_FRAME(sv.gameTime)     ← QVM game logic at 20Hz
```

**Key points:**
- `sv.time` / `svs.time` advance at `sv_fps` rate (60Hz = every 16ms)
- `sv.gameTime` / `level.time` advance at `sv_gameHz` rate (20Hz = every 50ms)
- Client usercmds arrive and are processed at `sv_fps` rate via `SV_ClientThink`
- `SV_BotFrame` was previously called before this loop at sv_fps rate — caused bot AI/movement desync. Now correctly placed inside the sv_gameHz inner loop.

### Client Think (sv_client.c — SV_ClientThink)

`sv_pmoveMsec` clamps the Pmove physics step to 8ms max. Without this, high-fps clients get finer collision resolution than low-fps clients (physics advantage).

**Problem:** The QVM's `ClientThink_real` uses the same `msec` value for both Pmove physics AND `ClientTimerActions` (bleed, bandage, inactivity timers). If we simply clamp `serverTime`, all timers run at 1/Nth speed where N = sv_fps/20.

**Solution:** Multi-step approach — fire multiple QVM calls per ucmd, each clamped to `maxStep`, until the full real delta is consumed. Pmove gets ≤8ms steps. Timers accumulate full real time across all steps.

**Critical safety:** Some QVM states skip Pmove entirely and don't advance `commandTime`:
- `level.intermissiontime` → `ClientIntermissionThink` → return (no Pmove)
- `level.pauseState` → return (no Pmove)

If we loop on these, the server hangs forever blocking all rcon and packet processing.

**Fix:** Check commandTime advancement on the FIRST call. If it doesn't advance, immediately fall through to single-call path via `goto single_call`. Never loop when QVM isn't running Pmove.

```c
// Simplified logic:
if ( delta > maxStep ) {
    prevCmdTime = commandTime;
    cl->lastUsercmd.serverTime = prevCmdTime + maxStep;
    VM_Call( GAME_CLIENT_THINK );
    
    if ( commandTime == prevCmdTime )
        goto single_call;   // intermission/paused — don't loop
    
    while ( realTime - commandTime > maxStep ) {
        // continue stepping...
        if ( commandTime didn't advance ) goto single_call;
    }
    // final partial step
    cl->lastUsercmd.serverTime = realTime;
    VM_Call( GAME_CLIENT_THINK );
    return;
}
single_call:
VM_Call( GAME_CLIENT_THINK );   // normal path
```

**Bots excluded** from sv_pmoveMsec clamping entirely. Bots set `cmd.serverTime = level.time` (50ms steps) and only get one `ClientThink_real` per game frame. Clamping their step causes them to only move 16% of intended distance per frame (warpy movement).

Check: `cl->netchan.remoteAddress.type != NA_BOT`

---

## Antilag (sv_antilag.c)

### Why Engine-Side, Not QVM

UT4.3's original antilag runs inside the QVM, recording player positions at `GAME_RUN_FRAME` time — i.e. at `sv_gameHz` rate (20Hz, every 50ms). At the original `sv_fps 20` this was fine: one snapshot per frame = perfect granularity.

At `sv_fps 60` with `sv_gameHz 20`, the QVM antilag still only records **one snapshot every 50ms**. A player moving at top speed covers significant distance in 50ms. When rewinding to `attackTime`, the engine can only snap to the nearest 50ms boundary — up to 50ms of positional error in the rewind. At 60Hz input, a shooter fires based on what they see (interpolated at 60Hz) but the rewind resolves against a position grid 3× coarser than their visual frame rate. This is unacceptable for competitive play.

Additionally, **we cannot modify the UT4.3 QVM antilag** — no source is available. Any fix must be transparent to the QVM.

Leaving QVM antilag active alongside engine antilag would double-compensate. The engine antilag replaces it entirely.

### Implementation

Engine-side position history. Records `sv_physicsScale` snapshots per engine tick (default 3 = 3 positions per 50ms = ~16ms granularity at sv_fps 60):

```
sv_fps 60, sv_physicsScale 3:
  → 180 position snapshots/sec
  → ~5.5ms granularity
  → rewind error < 1 frame at 60Hz input
```

Called from inside the `sv_fps` while loop in `SV_Frame`, before the `sv_gameHz` inner loop.

Rewinds entity positions to `attackTime` (from usercmd) for hit detection, then restores. Works transparently with QVM — QVM never knows positions were rewound.

---

## Player Visual Smoothing (sv_snapshot.c)

**Problem:** `BG_PlayerStateToEntityState()` sets `pos.trType = TR_INTERPOLATE` for all players. `TR_INTERPOLATE` ignores velocity entirely — `BG_EvaluateTrajectory` for `TR_INTERPOLATE` just returns `trBase` verbatim. Client lerps straight lines between snapshot positions.

At 20Hz this was a 50ms straight-line lerp, acceptable. At 60Hz it's 16ms segments, but fast direction changes still produce visible stutter because each segment is still a straight line with no velocity smoothing.

`BG_PlayerStateToEntityStateExtraPolate()` exists in the QVM and would fix this by setting `TR_LINEAR_STOP` with velocity, but it is **never called** anywhere in the game code.

**Fix (engine-side injection in SV_BuildCommonSnapshot):**

After copying `ent->s` into the snapshot entity array, override player entities:

```c
if ( entity.number < MAX_CLIENTS
    && entity.eType == ET_PLAYER
    && entity.pos.trType == TR_INTERPOLATE ) {
    entity.pos.trType     = TR_LINEAR_STOP;
    entity.pos.trTime     = svs.time;
    entity.pos.trDuration = snapMsec + 10;  // snapshot interval + buffer
    // trDelta (velocity) already set by BG_PlayerStateToEntityState — reuse it
}
```

The cgame `CG_CalcEntityLerpPositions` already handles `TR_LINEAR_STOP` for client entities — it calls `CG_InterpolateEntityPosition` which evaluates trajectory at both snap and nextSnap times using velocity. This path was dead because the server never sent `TR_LINEAR_STOP`. Now it works.

---

## Bot Fixes

### SV_BotFrame placement
**Before:** Called once per `sv_fps` tick, before game loop. 60 bot AI calls per second, 20 movement frames per second — 3x AI/movement ratio caused erratic bot behaviour.

**After:** Called inside `sv_gameHz` inner loop, immediately before `GAME_RUN_FRAME`. Bot AI and movement now tick in lockstep at 20Hz.

### sv_pmoveMsec bot exclusion
Bots excluded from step clamping as described above. Without this, bots with `cmd.serverTime = level.time` (50ms steps) would be clamped to 8ms and only move 16% of intended distance per game frame.

### Bot animation (KNOWN LIMITATION — not fixed)
Bot jump/land animations play from incorrect positions at `sv_fps > sv_gameHz`. Root cause: cgame `C_SetLerpFrameAnimation` sets `lerpFrame->animationTime = cg.time` (client time) when it detects an animation change. At 60Hz snapshots / 20Hz bot updates, `cg.time` when new animation arrives is up to 33ms ahead of when it actually started server-side.

Fix requires patching cgame QVM: `animationTime` should use `snap->serverTime` not `cg.time`. No QVM source available for 4.3. Real players unaffected. Documented as known limitation.

---

## QVM Constraints — Why sv_gameHz Must Stay at 20

The QVM has one hardcoded 20Hz assumption that would break gameplay if `sv_gameHz` were raised:

**`g_active.c` line ~1806 — antiwarp blank command injection:**

```c
// G_RunClient, called when a client is lagging (no fresh usercmd arrived)
ent->client->pers.cmd.serverTime += 50;  // HARDCODED: assumes 1 game frame = 50ms
ClientThink_real(ent);
```

At `sv_gameHz 40` (25ms frames) this injects a 50ms blank cmd instead of 25ms — teleporting lagging players forward by double the intended distance. This is a QVM issue, not fixable engine-side without QVM changes.

All other `+ 50` / `FRAMETIME` usages in the QVM are `nextthink` assignments compared against `level.time`. These are self-correcting at any `sv_gameHz` — they mean "fire next game tick" and do exactly that regardless of tick rate.

**`FRAMETIME` define** (`g_local.h: #define FRAMETIME 100`) is only used for `nextthink` assignments and is also self-correcting. Patching it would not fix anything.

---

## Known Issues / TODO

### End of round / map change (NEEDS TESTING)
The multi-step `commandTime` stall detection (the `goto single_call` fix) should resolve the reported freeze at end of round. `level.intermissiontime` causes `ClientThink_real` to return without Pmove, which previously caused an infinite loop in the multi-step code blocking the entire server thread. The `goto single_call` fix detects this on the first call and falls through immediately.

### rcon map load (NEEDS TESTING)
Was reported as broken. Likely the same root cause — rcon packets arrive via `Com_EventLoop` which runs before `SV_Frame`, but if the server thread was hung in the multi-step loop during intermission, rcon would time out. The intermission fix should resolve this too.

If rcon `map` still fails after the above fix, investigate:
- `CVAR_PROTECTED` on `sv_fps` — blocks console/rcon `set sv_fps` but `Cvar_Get` default still works
- `SV_TrackCvarChanges` called with partial state during map load

### Visual smoothness — fast strafe stutter (PARTIALLY FIXED)
The `TR_LINEAR_STOP` injection in `sv_snapshot.c` should reduce stutter on fast direction changes. Needs in-game testing to confirm the cgame is actually taking the `TR_LINEAR_STOP` interpolation path for these entities. If stutter persists, check:
- Whether `cg_smoothClients` cvar value matters (it forces `TR_INTERPOLATE` back if 0)
- The `cg.frameInterpolation` value range at 60Hz
- Whether `cg.nextSnap` is consistently available (extrapolation fallback if not)

### Items not yet investigated
- Whether `sv_snapshotFps < sv_fps` causes any issues with the TR_LINEAR_STOP `trDuration` calculation
- Long session time wrap (`sv.time > 0x78000000`) behaviour with antilag history

---

## Files Modified (engine only — no QVM changes)

| File | Changes |
|------|---------|
| `sv_main.c` | sv_fps/sv_gameHz frame loop decoupling; timeResidual hard clamp; SV_BotFrame moved to sv_gameHz inner loop; antilag sub-tick recording |
| `sv_init.c` | Register sv_gameHz, sv_snapshotFps, sv_busyWait, sv_pmoveMsec; antilag cvar registration; CVG_SERVER group tracking |
| `sv_client.c` | sv_pmoveMsec multi-step loop with commandTime stall detection; bot exclusion from clamping |
| `sv_snapshot.c` | TR_LINEAR_STOP velocity injection for player entities in SV_BuildCommonSnapshot |
| `sv_antilag.c` | Full engine-side antilag implementation |
| `sv_antilag.h` | Antilag interface |

---

## Testing Priorities

1. **End of round → next map** — does it proceed without freeze?
2. **rcon `map <mapname>`** during live game — does it execute?
3. **Bleed/bandage timing** — does a bleeding player bleed out at the correct rate vs vanilla?
4. **Fast strafe visual smoothness** — is the stutter reduced with TR_LINEAR_STOP?
5. **Bot movement** — smooth at all sv_fps values?
6. **Weapon ROF / reload** — correct at 60fps clients?
7. **Stamina drain rate** — correct at 60fps (slide drain is `pml.msec / 8.0` — exactly calibrated)?

---

## Reference: pml.msec Normalisation

All Pmove-internal rate-dependent calculations use `pml.msec` (the physics step size). With `sv_pmoveMsec 8` and multi-step calls:

- `weaponTime -= pml.msec` → correct, accumulates across steps
- `STAT_RECOIL -= recoil.fall * pml.msec` → correct
- `STAT_STAMINA` drain → correct (`slide drain = DRAIN * pml.msec/8.0` exactly calibrated)
- `bobCycle += bobmove * pml.msec` → correct, boundary-crossing footstep detection is rate-independent
- `ClientTimerActions(ent, msec)` where `msec = ucmd->serverTime - commandTime` → correct, accumulates full real elapsed time across multi-step calls

Rate-dependent calculations that do NOT use `pml.msec` (use level.time instead):
- All `nextthink` assignments → self-correcting, fine at any sv_gameHz
- Bleed/bandage timers in `ClientTimerActions` → now correct via multi-step
- Inactivity timer → uses `level.time` comparisons, fine
