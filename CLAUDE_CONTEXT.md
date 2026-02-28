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
  sv_snapshot.c       ← engine-side player position extrapolation (sv.time - sv.gameTime offset)
  sv_ccmds.c          ← SV_MapRestart_f: sync sv.gameTime on restart, GAME_RUN_FRAME uses sv.gameTime
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
| `sv_snapshotFps` | `-1` | Snapshot send rate to clients (-1 = match sv_fps) |
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

           emptyFrame = true  ← reset per sv_fps tick for USE_MV multiview recorder
           gameTimeResidual += frameMsec
           while gameTimeResidual >= gameMsec (1000/sv_gameHz):
               gameTimeResidual -= gameMsec
               sv.gameTime += gameMsec
               SV_BotFrame(sv.gameTime)        ← IMPORTANT: bot AI ticks here, in lockstep
               GAME_RUN_FRAME(sv.gameTime)     ← QVM game logic at 20Hz
               emptyFrame = false              ← signals multiview to record this tick

           SV_IssueNewSnapshot()   ← per sv_fps tick
           SV_SendClientMessages() ← per sv_fps tick ← KEY: not once per Com_Frame

       SV_CheckTimeouts()  ← once per Com_Frame is sufficient
```

**Key points:**
- `sv.time` / `svs.time` advance at `sv_fps` rate (60Hz = every 16ms)
- `sv.gameTime` / `level.time` advance at `sv_gameHz` rate (20Hz = every 50ms)
- Client usercmds arrive and are processed at `sv_fps` rate via `SV_ClientThink`
- `SV_BotFrame` was previously called before this loop at sv_fps rate — caused bot AI/movement desync. Now correctly placed inside the sv_gameHz inner loop.
- `SV_SendClientMessages` moved inside the sv_fps loop — see Packet Flow section below.

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

## Packet Flow and Snapshot Dispatch

### Root Cause of Netgraph Jitter (Late/Early Packets)

The original code called `SV_IssueNewSnapshot()` + `SV_SendClientMessages()` **once per `Com_Frame`** call, outside the sv_fps tick loop. `Com_Frame` arrives at irregular intervals due to OS scheduler jitter. When the OS sleeps too long and delivers a `Com_Frame` that's 32ms late instead of 16ms, two sv_fps ticks fire inside the loop — but only one `SV_SendClientMessages` call fires at the end:

```
Double-tick scenario (OS sleep overshoot):
  Com_Frame at t=0ms:  timeResidual=10ms → 0 ticks, 0 snapshots
  Com_Frame at t=34ms: timeResidual=34ms → 2 ticks fire (svs.time → 32ms)
                       SV_SendClientMessages fires once → 1 snapshot sent
                       Client sees a 32ms gap where two 16ms gaps should be
```

The snapshotMsec gate (`svs.time - lastSnapshotTime >= snapshotMsec`) passes in the double-tick case (32ms >= 16ms), but it only fires once — the second snapshot for the second tick is simply never generated. This is the netgraph late-packet spike.

On Linux with `sv_busyWait 0`: low frequency, ±1-2ms jitter. Occasional. On Windows with 15.625ms default timer resolution: very common.

### Fix: Snapshot Dispatch Inside the sv_fps Loop

`SV_IssueNewSnapshot()` + `SV_SendClientMessages()` moved inside the `while (timeResidual >= frameMsec)` loop (sv_main.c). Each engine tick now dispatches its own snapshot opportunity — two ticks produce two dispatch attempts, regardless of how the OS scheduled `Com_Frame`.

Clients with a finite `rate` cvar (e.g., `rate 25000`) are naturally protected from burst by the `SV_RateMsec` bandwidth limiter inside `SV_SendClientMessages`. With `rate 0` (unlimited, typical for competitive LAN/dedicated), both snapshots go through.

`SV_CheckTimeouts` remains outside the loop (once per Com_Frame is fine for timeout detection).

### USE_MV Multiview Recorder Interaction

`svs.emptyFrame` is now reset to `qtrue` at the start of each sv_fps tick (inside the loop) instead of once before the loop. It's set to `qfalse` when a `GAME_RUN_FRAME` fires (at sv_gameHz rate). The multiview recorder inside `SV_SendClientMessages` checks `!emptyFrame` — so it still records only at sv_gameHz rate (~20Hz), on the specific sv_fps ticks where a game frame actually fired.

### cl_maxpackets — Upstream, Different Problem

`cl_maxpackets` limits how often the **client sends input to the server** (upstream). The netgraph shows **server → client** snapshot timing (downstream). `cl_maxpackets` does NOT cause the netgraph late/early packets.

At `sv_fps 60` with `cl_maxpackets 30` (UT default): server processes 60 input ticks/sec but receives fresh usercmds only 30/sec. The server reuses the last usercmd for the in-between ticks. This doesn't cause netgraph jitter but means the bot velocity extrapolation and the real-player ps->origin are both slightly stale for 50% of ticks.

**Recommendation for players:** set `cl_maxpackets 60` or `cl_maxpackets 125` when playing on a 60Hz sv_fps server. The server cannot force this.

---

## Player Visual Smoothing

### Root Cause of Stutter at sv_fps > sv_gameHz

`BG_PlayerStateToEntityState` is called only inside `ClientEndFrame`, which runs at the end of each `GAME_RUN_FRAME` — i.e., at `sv_gameHz` rate (20Hz, every 50ms). This means `ent->s.pos.trBase` (the entity position in outbound snapshots) only updates 20 times per second.

At `sv_fps 60 / sv_snapshotFps 60`, three consecutive snapshots go out between each game frame. Without correction:
- snap at t=0ms: position P₀ (fresh game frame)
- snap at t=16ms: position P₀ (stale — unchanged)
- snap at t=33ms: position P₀ (stale — unchanged)
- snap at t=50ms: position P₁ (new game frame — jump)

The cgame interpolates these and sees two dead frames followed by a sudden jump. This is the visual stutter.

### Fix: Engine-Side Position Fixup (sv_snapshot.c)

`SV_BuildCommonSnapshot` corrects player entity positions between game frames before the snapshot is stamped. The approach differs by entity type:

**Real players** — `ps->origin` is already the correct post-Pmove position (updated by `SV_ClientThink` every usercmd). We copy it directly:
```c
const playerState_t *ps = SV_GameClientNum( es->number );
VectorCopy( ps->origin, es->pos.trBase );
VectorCopy( ps->velocity, es->pos.trDelta );
```

**Bots** — `ps->origin` only updates at game-frame boundaries (bot AI ticks at sv_gameHz). Between frames we use velocity extrapolation:
```c
const float dt = extrapolateMs * 0.001f;
es->pos.trBase[0] += es->pos.trDelta[0] * dt;
es->pos.trBase[1] += es->pos.trDelta[1] * dt;
es->pos.trBase[2] += es->pos.trDelta[2] * dt;
```

**Investigated and rejected: TR_LINEAR_STOP injection**

An earlier approach injected `TR_LINEAR_STOP` with velocity to have the cgame extrapolate forward. Found to be completely ineffective:

1. `CG_CalcEntityLerpPositions` (cg_ents.c) checks `cg_smoothClients` first. Default is 0, which immediately overwrites any `TR_LINEAR_STOP` back to `TR_INTERPOLATE` for all client entities.
2. Even with `cg_smoothClients 1`, TR_LINEAR_STOP player entities hit `CG_InterpolateEntityPosition` which evaluates at `cg.snap->serverTime` — `deltaTime = 0`, velocity contribution zeroed.

The `TR_LINEAR_STOP` injection was removed. Engine-side extrapolation (modifying `trBase` before stamping the snapshot) is the correct approach.

**Client `snaps` cvar (default 20)**

The client sends `snaps 20` in userinfo, which in vanilla Q3/UT would cap the server to 20 snapshots/sec for that client. Our engine ignores this — `sv_snapshotFps` is fully authoritative and all clients receive at the server-controlled rate regardless of their `snaps` value (sv_client.c `SV_UserinfoChanged`).

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

### Visual smoothness — position extrapolation (IMPLEMENTED, NEEDS TESTING)
Engine-side position extrapolation in `SV_BuildCommonSnapshot` (sv_snapshot.c) should eliminate the 3-snapshot stutter pattern. Needs in-game testing with multiple human players at `sv_fps 60 / sv_gameHz 20`. If residual stutter remains, check:
- Whether velocity in `trDelta` is zeroed for crouching or specific movement states in UT4.3 QVM
- Whether `sv.time - sv.gameTime` ever exceeds `1000/sv_gameHz` ms (should be bounded by the inner loop logic)

### sv_ccmds.c — GAME_RUN_FRAME time clock (FIXED)
`SV_MapRestart_f` was calling `GAME_RUN_FRAME` with `sv.time` instead of `sv.gameTime` in both its warmup loop (3 settle frames) and final frame. Fixed to match the `SV_SpawnServer` pattern: sync `sv.gameTime = sv.time` and `sv.gameTimeResidual = 0` before `SV_RestartGameProgs()`, then advance and call in lockstep.

### sv_smoothClients / sv_bufferMs stationary jitter (FIXED)
Both `sv_smoothClients 1` and `sv_bufferMs -1` caused visible vibration on stationary players due to Pmove ground-snapping micro-oscillations tripping the dead-zone threshold. Fixed by raising dead-zone from `DotProduct > 1.0` to `DotProduct > 100.0` (10 ups) and adding the same guard to the `sv_bufferMs` ring buffer path. Both features default to off (0).

### cg_smoothClients QVM intercept (IMPLEMENTED)
Added `cg_smoothClients` to the cgame `CG_CVAR_SET` intercept in `cl_cgame.c`, same pattern as `snaps`. Prevents QVM from forcing its default. Engine controls the default — currently not needed since `sv_smoothClients 0` sends `TR_INTERPOLATE` anyway, but enables future testing without QVM modification.

### frameInterpolation clamp — VM intercept not possible
Investigated using `trap_Cvar_Set` intercept to fix the unclamped `frameInterpolation` in the cgame. Not possible — the computation is entirely internal to the QVM (no syscall). Only fixable via Ghidra binary patch (documented in `docs/ghidra-cgame-patches.md`) or by ensuring snapshots arrive fast enough that `cg.time` doesn't overshoot (current approach via `sv_extrapolate 1`).

### Items not yet investigated
- Long session time wrap (`sv.time > 0x78000000`) behaviour with antilag history

---

## Files Modified (engine only — no QVM changes)

| File | Changes |
|------|---------|
| `sv_main.c` | sv_fps/sv_gameHz frame loop decoupling; timeResidual hard clamp; SV_BotFrame moved to sv_gameHz inner loop; antilag sub-tick recording |
| `sv_init.c` | Register sv_gameHz, sv_snapshotFps, sv_busyWait, sv_pmoveMsec; antilag cvar registration; CVG_SERVER group tracking |
| `sv_client.c` | sv_pmoveMsec multi-step loop with commandTime stall detection; bot exclusion from clamping |
| `sv_snapshot.c` | `snaps` client cvar bypassed — sv_snapshotFps is authoritative; real players use ps->origin for position fixup between game frames; bots use velocity extrapolation (trDelta * dt) |
| `sv_ccmds.c` | `SV_MapRestart_f`: sync `sv.gameTime = sv.time` + `sv.gameTimeResidual = 0` on restart; warmup frames pass `sv.gameTime` to `GAME_RUN_FRAME` (was incorrectly using `sv.time`) |
| `sv_antilag.c` | Full engine-side antilag implementation |
| `sv_antilag.h` | Antilag interface |
| `cl_cgame.c` | `CG_CVAR_SET` intercept: blocks QVM from setting `snaps`, `cg_smoothClients` |

---

## Testing Priorities

1. **End of round → next map** — does it proceed without freeze?
2. **rcon `map <mapname>`** during live game — does it execute?
3. **Bleed/bandage timing** — does a bleeding player bleed out at the correct rate vs vanilla?
4. **Fast strafe visual smoothness** — is the stutter reduced with engine-side position extrapolation?
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
